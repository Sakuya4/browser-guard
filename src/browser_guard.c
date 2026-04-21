#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "browser_guard.h"
#include "process_control.h"

#define BG_OVERLAY_CLASS_NAME L"BrowserGuardOverlayWindow"
#define BG_OVERLAY_WIDTH 320
#define BG_OVERLAY_HEIGHT 64

typedef struct TrackedProcess {
    DWORD pid;
    wchar_t exe_name[MAX_PATH];
    bool suspended;
    bool seen_this_pass;
    DWORD last_trim_tick;
} TrackedProcess;

static volatile BOOL g_should_stop = FALSE;
static volatile BOOL g_resume_requested = FALSE;
static HWND g_overlay_window = NULL;
static HWND g_resume_target_window = NULL;
static HWND g_overlay_owner_window = NULL;

typedef struct OverlayState {
    bool visible;
    unsigned int suspended_count;
    HWND target_window;
} OverlayState;

static LRESULT CALLBACK overlay_window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_LBUTTONUP:
            g_resume_requested = TRUE;
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT paint;
            RECT rect;
            HDC dc = BeginPaint(hwnd, &paint);
            HFONT font = NULL;
            HBRUSH background = CreateSolidBrush(RGB(30, 36, 48));
            SetRect(&rect, 0, 0, BG_OVERLAY_WIDTH, BG_OVERLAY_HEIGHT);
            FillRect(dc, &rect, background);
            DeleteObject(background);

            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, RGB(240, 244, 248));
            font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            if (font != NULL) {
                SelectObject(dc, font);
            }

            GetClientRect(hwnd, &rect);
            DrawTextW(
                dc,
                L"Browser paused.\nClick here or click the browser to resume.",
                -1,
                &rect,
                DT_LEFT | DT_VCENTER | DT_WORDBREAK
            );
            EndPaint(hwnd, &paint);
            return 0;
        }
        default:
            return DefWindowProcW(hwnd, message, wparam, lparam);
    }
}

static BOOL WINAPI handle_console_signal(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT || signal == CTRL_CLOSE_EVENT) {
        g_should_stop = TRUE;
        return TRUE;
    }

    return FALSE;
}

static TrackedProcess *find_tracked_process(TrackedProcess *tracked, size_t tracked_count, DWORD pid) {
    for (size_t i = 0; i < tracked_count; ++i) {
        if (tracked[i].pid == pid) {
            return &tracked[i];
        }
    }

    return NULL;
}

static bool register_overlay_window_class(void) {
    WNDCLASSEXW window_class;

    ZeroMemory(&window_class, sizeof(window_class));
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = overlay_window_proc;
    window_class.hInstance = GetModuleHandleW(NULL);
    window_class.lpszClassName = BG_OVERLAY_CLASS_NAME;
    window_class.hCursor = LoadCursorW(NULL, IDC_HAND);
    window_class.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    return RegisterClassExW(&window_class) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

static bool create_overlay_window(void) {
    if (!register_overlay_window_class()) {
        return false;
    }

    g_overlay_window = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        BG_OVERLAY_CLASS_NAME,
        L"browser_guard",
        WS_POPUP,
        16,
        16,
        BG_OVERLAY_WIDTH,
        BG_OVERLAY_HEIGHT,
        NULL,
        NULL,
        GetModuleHandleW(NULL),
        NULL
    );

    return g_overlay_window != NULL;
}

static void destroy_overlay_window(void) {
    if (g_overlay_window != NULL) {
        DestroyWindow(g_overlay_window);
        g_overlay_window = NULL;
    }
}

static void update_overlay_window(const OverlayState *state) {
    RECT target_rect;

    if (g_overlay_window == NULL) {
        return;
    }

    if (state->visible &&
        state->target_window != NULL &&
        IsWindow(state->target_window) &&
        !IsIconic(state->target_window) &&
        GetWindowRect(state->target_window, &target_rect)) {
        int x = target_rect.left + 8;
        int y = target_rect.top + 8;

        g_overlay_owner_window = state->target_window;
        SetWindowPos(
            g_overlay_window,
            g_overlay_owner_window,
            x,
            y,
            BG_OVERLAY_WIDTH,
            BG_OVERLAY_HEIGHT,
            SWP_NOACTIVATE | SWP_SHOWWINDOW
        );
        ShowWindow(g_overlay_window, SW_SHOWNOACTIVATE);
        InvalidateRect(g_overlay_window, NULL, TRUE);
        UpdateWindow(g_overlay_window);
    } else {
        ShowWindow(g_overlay_window, SW_HIDE);
        g_overlay_owner_window = NULL;
    }
}

static bool is_pid_suspended(const TrackedProcess *tracked, size_t tracked_count, DWORD pid) {
    for (size_t i = 0; i < tracked_count; ++i) {
        if (tracked[i].pid == pid && tracked[i].suspended) {
            return true;
        }
    }

    return false;
}

static bool clicked_suspended_browser_window(
    const SecurityContext *security_context,
    const TrackedProcess *tracked,
    size_t tracked_count,
    HWND *target_browser_window
) {
    static SHORT previous_left_button_state = 0;
    SHORT current_state = GetAsyncKeyState(VK_LBUTTON);

    if ((current_state & 0x8000) == 0 || (previous_left_button_state & 0x8000) != 0) {
        previous_left_button_state = current_state;
        return false;
    }

    previous_left_button_state = current_state;

    POINT cursor;
    HWND target_window;
    DWORD pid = 0;

    if (!GetCursorPos(&cursor)) {
        return false;
    }

    target_window = WindowFromPoint(cursor);
    if (target_window == NULL) {
        return false;
    }

    target_window = GetAncestor(target_window, GA_ROOT);
    if (target_window == g_overlay_window) {
        return false;
    }

    GetWindowThreadProcessId(target_window, &pid);
    if (pid == 0 || !is_pid_suspended(tracked, tracked_count, pid)) {
        return false;
    }

    if (!is_manageable_browser_pid(security_context, pid)) {
        return false;
    }

    *target_browser_window = target_window;
    return true;
}

static void pump_ui_messages(
    const SecurityContext *security_context,
    const TrackedProcess *tracked,
    size_t tracked_count,
    DWORD interval_ms
) {
    DWORD elapsed = 0;
    const DWORD slice_ms = 50;

    while (!g_should_stop && elapsed < interval_ms) {
        MSG message;

        while (PeekMessageW(&message, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        HWND target_browser_window = NULL;
        if (clicked_suspended_browser_window(security_context, tracked, tracked_count, &target_browser_window)) {
            g_resume_target_window = target_browser_window;
            g_resume_requested = TRUE;
            return;
        }

        Sleep(slice_ms);
        elapsed += slice_ms;
    }
}

static void resume_all_tracked(TrackedProcess *tracked, size_t tracked_count, const AppConfig *config) {
    for (size_t i = 0; i < tracked_count; ++i) {
        if (tracked[i].suspended) {
            set_process_suspended(tracked[i].pid, false, config);
            tracked[i].suspended = false;
        }
    }
}

static void cleanup_tracked_processes(TrackedProcess *tracked, size_t *tracked_count, const AppConfig *config) {
    size_t write_index = 0;

    for (size_t i = 0; i < *tracked_count; ++i) {
        if (!tracked[i].seen_this_pass) {
            if (tracked[i].suspended) {
                set_process_suspended(tracked[i].pid, false, config);
            }
            continue;
        }

        tracked[i].seen_this_pass = false;
        tracked[write_index++] = tracked[i];
    }

    *tracked_count = write_index;
}

static unsigned int count_suspended_processes(const TrackedProcess *tracked, size_t tracked_count) {
    unsigned int suspended_count = 0;

    for (size_t i = 0; i < tracked_count; ++i) {
        if (tracked[i].suspended) {
            suspended_count += 1;
        }
    }

    return suspended_count;
}

static HWND choose_overlay_target_window(
    const BrowserGroup *groups,
    size_t group_count,
    const TrackedProcess *tracked,
    size_t tracked_count
) {
    for (size_t i = 0; i < group_count; ++i) {
        if (groups[i].anchor_window == NULL) {
            continue;
        }

        for (size_t j = 0; j < groups[i].pid_count; ++j) {
            if (is_pid_suspended(tracked, tracked_count, groups[i].pids[j])) {
                return groups[i].anchor_window;
            }
        }
    }

    return NULL;
}

static void log_memory_totals(const BrowserGroup *groups, size_t group_count) {
    MemoryTotals totals;

    if (!snapshot_memory_totals(groups, group_count, &totals)) {
        return;
    }

    printf(
        "[memory] processes=%zu working_set=%.2f MB private=%.2f MB\n",
        totals.process_count,
        (double)totals.working_set_bytes / (1024.0 * 1024.0),
        (double)totals.private_bytes / (1024.0 * 1024.0)
    );
}

static void ensure_group_state(
    const BrowserGroup *group,
    TrackedProcess *tracked,
    size_t *tracked_count,
    size_t tracked_capacity,
    const AppConfig *config,
    DWORD now_tick
) {
    bool should_suspend = !group->has_foreground_window && !group->has_audio;

    for (size_t i = 0; i < group->pid_count; ++i) {
        DWORD pid = group->pids[i];
        TrackedProcess *entry = find_tracked_process(tracked, *tracked_count, pid);

        if (entry == NULL) {
            if (*tracked_count >= tracked_capacity) {
                continue;
            }

            entry = &tracked[*tracked_count];
            ZeroMemory(entry, sizeof(*entry));
            entry->pid = pid;
            wcsncpy(entry->exe_name, group->exe_name, MAX_PATH - 1);
            *tracked_count += 1;
        }

        entry->seen_this_pass = true;

        if (entry->suspended != should_suspend) {
            if (!set_process_suspended(pid, should_suspend, config)) {
                if (config->verbose) {
                    fwprintf(stdout, L"[warn] unable to change pid=%lu (%ls)\n", pid, group->exe_name);
                }
                continue;
            }

            entry->suspended = should_suspend;
            entry->last_trim_tick = now_tick;
            if (config->verbose) {
                fwprintf(
                    stdout,
                    should_suspend ? L"[suspend] pid=%lu (%ls)\n" : L"[resume] pid=%lu (%ls)\n",
                    pid,
                    group->exe_name
                );
            }
            continue;
        }

        if (should_suspend &&
            config->trim_working_set &&
            now_tick - entry->last_trim_tick >= config->trim_interval_ms) {
            maintain_suspended_process(pid, config);
            entry->last_trim_tick = now_tick;
            if (config->verbose) {
                fwprintf(stdout, L"[trim] pid=%lu (%ls)\n", pid, group->exe_name);
            }
        }
    }
}

int run_browser_guard(const AppConfig *config) {
    BrowserGroup groups[BG_MAX_BROWSER_GROUPS];
    TrackedProcess tracked[BG_MAX_TRACKED_PROCESSES];
    SecurityContext security_context;
    HRESULT hr = S_OK;
    size_t tracked_count = 0;
    OverlayState overlay_state;

    ZeroMemory(tracked, sizeof(tracked));
    ZeroMemory(&overlay_state, sizeof(overlay_state));
    if (!init_security_context(&security_context)) {
        fprintf(stderr, "Failed to initialize process ownership checks.\n");
        return 1;
    }

    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        fprintf(stderr, "CoInitializeEx failed: 0x%08lx\n", (unsigned long)hr);
        return 1;
    }

    if (!SetConsoleCtrlHandler(handle_console_signal, TRUE)) {
        fprintf(stderr, "Failed to install console control handler.\n");
        CoUninitialize();
        return 1;
    }

    if (!create_overlay_window()) {
        fprintf(stderr, "Failed to create overlay window.\n");
        CoUninitialize();
        return 1;
    }

    while (!g_should_stop) {
        size_t group_count = 0;
        DWORD now_tick = GetTickCount();

        if (g_resume_requested) {
            resume_all_tracked(tracked, tracked_count, config);
            if (g_resume_target_window != NULL) {
                ShowWindow(g_resume_target_window, SW_RESTORE);
                SetForegroundWindow(g_resume_target_window);
                g_resume_target_window = NULL;
            }
            g_resume_requested = FALSE;
        }

        ZeroMemory(groups, sizeof(groups));
        collect_browser_groups(&security_context, groups, &group_count, BG_MAX_BROWSER_GROUPS);
        mark_foreground_groups(groups, group_count);
        mark_audio_groups(groups, group_count);

        for (size_t i = 0; i < group_count; ++i) {
            ensure_group_state(&groups[i], tracked, &tracked_count, BG_MAX_TRACKED_PROCESSES, config, now_tick);
        }

        if (config->verbose) {
            log_memory_totals(groups, group_count);
        }

        cleanup_tracked_processes(tracked, &tracked_count, config);
        overlay_state.suspended_count = count_suspended_processes(tracked, tracked_count);
        overlay_state.target_window = choose_overlay_target_window(groups, group_count, tracked, tracked_count);
        overlay_state.visible = overlay_state.suspended_count > 0 && overlay_state.target_window != NULL;
        update_overlay_window(&overlay_state);
        pump_ui_messages(&security_context, tracked, tracked_count, config->interval_ms);
    }

    update_overlay_window(&(OverlayState){0});
    resume_all_tracked(tracked, tracked_count, config);
    destroy_overlay_window();
    CoUninitialize();
    return 0;
}
