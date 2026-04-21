#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "browser_guard.h"
#include "process_control.h"

typedef struct TrackedProcess {
    DWORD pid;
    wchar_t exe_name[MAX_PATH];
    bool suspended;
    bool seen_this_pass;
    DWORD last_trim_tick;
} TrackedProcess;

static volatile BOOL g_should_stop = FALSE;

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

    ZeroMemory(tracked, sizeof(tracked));
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

    while (!g_should_stop) {
        size_t group_count = 0;
        DWORD now_tick = GetTickCount();

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
        Sleep(config->interval_ms);
    }

    resume_all_tracked(tracked, tracked_count, config);
    CoUninitialize();
    return 0;
}
