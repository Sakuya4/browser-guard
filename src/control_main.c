#include <windows.h>
#include <shellapi.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <strsafe.h>
#include <tlhelp32.h>
#include <wchar.h>

#define BG_CONTROL_CLASS_NAME L"BrowserGuardControlWindow"
#define BG_NOTIFICATION_CLASS_NAME L"BrowserGuardControlNotification"
#define BG_INSTALLED_EXE_NAME L"browser_guard.exe"
#define BG_DISABLED_FILE_NAME L"browser_guard.disabled"
#define BG_ARGUMENTS_FILE_NAME L"browser_guard.args.txt"
#define BG_NOTIFICATION_TIMEOUT_MS 2200
#define BG_NOTIFICATION_WIDTH 360
#define BG_NOTIFICATION_HEIGHT 108
#define BG_NOTIFICATION_MARGIN 16
#define BG_NOTIFICATION_TITLE_HEIGHT 28

typedef struct NotificationWindowData {
    const wchar_t *title;
    const wchar_t *message;
    COLORREF background_color;
    COLORREF border_color;
    COLORREF title_color;
    COLORREF body_color;
} NotificationWindowData;

typedef enum ControlMode {
    CONTROL_MODE_TOGGLE,
    CONTROL_MODE_LAUNCH
} ControlMode;

static LRESULT CALLBACK control_window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    (void)hwnd;
    (void)wparam;
    (void)lparam;

    switch (message) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hwnd, message, wparam, lparam);
    }
}

static HFONT get_notification_font(bool title_font) {
    NONCLIENTMETRICSW metrics;
    LOGFONTW font_description;

    ZeroMemory(&metrics, sizeof(metrics));
    metrics.cbSize = sizeof(metrics);
    if (!SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0)) {
        return (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    }

    font_description = metrics.lfMessageFont;
    if (title_font) {
        font_description.lfWeight = FW_SEMIBOLD;
        font_description.lfHeight -= 1;
    }

    return CreateFontIndirectW(&font_description);
}

static LRESULT CALLBACK notification_window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    NotificationWindowData *data = (NotificationWindowData *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (message) {
        case WM_NCCREATE: {
            CREATESTRUCTW *create = (CREATESTRUCTW *)lparam;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)create->lpCreateParams);
            return TRUE;
        }
        case WM_TIMER:
            DestroyWindow(hwnd);
            return 0;
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
            DestroyWindow(hwnd);
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT paint;
            RECT client_rect;
            RECT title_rect;
            RECT body_rect;
            HDC dc = BeginPaint(hwnd, &paint);
            HBRUSH background_brush;
            HPEN border_pen;
            HFONT title_font = NULL;
            HFONT body_font = NULL;
            HFONT previous_font = NULL;
            int previous_mode = SetBkMode(dc, TRANSPARENT);

            if (data == NULL) {
                EndPaint(hwnd, &paint);
                return 0;
            }

            GetClientRect(hwnd, &client_rect);
            background_brush = CreateSolidBrush(data->background_color);
            FillRect(dc, &client_rect, background_brush);
            DeleteObject(background_brush);

            border_pen = CreatePen(PS_SOLID, 1, data->border_color);
            SelectObject(dc, border_pen);
            Rectangle(dc, client_rect.left, client_rect.top, client_rect.right, client_rect.bottom);
            DeleteObject(border_pen);

            title_rect = client_rect;
            title_rect.left += 16;
            title_rect.top += 12;
            title_rect.right -= 16;
            title_rect.bottom = title_rect.top + BG_NOTIFICATION_TITLE_HEIGHT;

            body_rect = client_rect;
            body_rect.left += 16;
            body_rect.top = title_rect.bottom + 2;
            body_rect.right -= 16;
            body_rect.bottom -= 14;

            title_font = get_notification_font(true);
            previous_font = (HFONT)SelectObject(dc, title_font);
            SetTextColor(dc, data->title_color);
            DrawTextW(dc, data->title, -1, &title_rect, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

            body_font = get_notification_font(false);
            SelectObject(dc, body_font);
            SetTextColor(dc, data->body_color);
            DrawTextW(dc, data->message, -1, &body_rect, DT_LEFT | DT_WORDBREAK | DT_END_ELLIPSIS);

            SelectObject(dc, previous_font);
            if (title_font != NULL && title_font != GetStockObject(DEFAULT_GUI_FONT)) {
                DeleteObject(title_font);
            }
            if (body_font != NULL && body_font != GetStockObject(DEFAULT_GUI_FONT)) {
                DeleteObject(body_font);
            }
            SetBkMode(dc, previous_mode);
            EndPaint(hwnd, &paint);
            return 0;
        }
        default:
            return DefWindowProcW(hwnd, message, wparam, lparam);
    }
}

static bool get_module_directory(wchar_t *buffer, size_t buffer_count) {
    DWORD length = GetModuleFileNameW(NULL, buffer, (DWORD)buffer_count);
    wchar_t *slash = NULL;

    if (length == 0 || length >= buffer_count) {
        return false;
    }

    slash = wcsrchr(buffer, L'\\');
    if (slash == NULL) {
        return false;
    }

    *slash = L'\0';
    return true;
}

static bool build_path(wchar_t *buffer, size_t buffer_count, const wchar_t *directory, const wchar_t *file_name) {
    return SUCCEEDED(StringCchPrintfW(buffer, buffer_count, L"%ls\\%ls", directory, file_name));
}

static unsigned int count_guard_processes(void) {
    unsigned int count = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W entry;

    if (snapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }

    ZeroMemory(&entry, sizeof(entry));
    entry.dwSize = sizeof(entry);

    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, BG_INSTALLED_EXE_NAME) == 0) {
                count += 1;
            }
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return count;
}

static bool terminate_guard_processes(void) {
    bool success = true;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W entry;

    if (snapshot == INVALID_HANDLE_VALUE) {
        return false;
    }

    ZeroMemory(&entry, sizeof(entry));
    entry.dwSize = sizeof(entry);

    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, BG_INSTALLED_EXE_NAME) != 0) {
                continue;
            }

            HANDLE process = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, entry.th32ProcessID);
            if (process == NULL) {
                success = false;
                continue;
            }

            if (!TerminateProcess(process, 0)) {
                success = false;
                CloseHandle(process);
                continue;
            }

            WaitForSingleObject(process, 2000);
            CloseHandle(process);
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return success;
}

static bool write_disabled_flag(const wchar_t *path, bool disabled) {
    if (!disabled) {
        DeleteFileW(path);
        return true;
    }

    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    CloseHandle(file);
    return true;
}

static bool is_disabled(const wchar_t *path) {
    DWORD attributes = GetFileAttributesW(path);
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static bool read_arguments_file(const wchar_t *path, wchar_t *buffer, size_t buffer_count) {
    HANDLE file = INVALID_HANDLE_VALUE;
    DWORD bytes_read = 0;
    char bytes[2048];
    int wide_length = 0;

    buffer[0] = L'\0';
    file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return true;
    }

    if (!ReadFile(file, bytes, sizeof(bytes) - 1, &bytes_read, NULL)) {
        CloseHandle(file);
        return false;
    }

    CloseHandle(file);
    bytes[bytes_read] = '\0';

    if (bytes_read >= 3 &&
        (unsigned char)bytes[0] == 0xEF &&
        (unsigned char)bytes[1] == 0xBB &&
        (unsigned char)bytes[2] == 0xBF) {
        memmove(bytes, bytes + 3, bytes_read - 2);
        bytes_read -= 3;
    }

    while (bytes_read > 0 && (bytes[bytes_read - 1] == '\r' || bytes[bytes_read - 1] == '\n')) {
        bytes[--bytes_read] = '\0';
    }

    wide_length = MultiByteToWideChar(CP_UTF8, 0, bytes, -1, buffer, (int)buffer_count);
    return wide_length > 0;
}

static bool start_guard_process(const wchar_t *install_directory) {
    wchar_t exe_path[MAX_PATH];
    wchar_t args_path[MAX_PATH];
    wchar_t args_buffer[2048];
    wchar_t command_line[4096];
    STARTUPINFOW startup_info;
    PROCESS_INFORMATION process_info;

    if (!build_path(exe_path, MAX_PATH, install_directory, BG_INSTALLED_EXE_NAME)) {
        return false;
    }
    if (!build_path(args_path, MAX_PATH, install_directory, BG_ARGUMENTS_FILE_NAME)) {
        return false;
    }
    if (GetFileAttributesW(exe_path) == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    if (!read_arguments_file(args_path, args_buffer, sizeof(args_buffer) / sizeof(args_buffer[0]))) {
        return false;
    }

    if (args_buffer[0] == L'\0') {
        if (FAILED(StringCchPrintfW(command_line, sizeof(command_line) / sizeof(command_line[0]), L"\"%ls\"", exe_path))) {
            return false;
        }
    } else {
        if (FAILED(StringCchPrintfW(command_line, sizeof(command_line) / sizeof(command_line[0]), L"\"%ls\" %ls", exe_path, args_buffer))) {
            return false;
        }
    }

    ZeroMemory(&startup_info, sizeof(startup_info));
    startup_info.cb = sizeof(startup_info);
    ZeroMemory(&process_info, sizeof(process_info));

    if (!CreateProcessW(
        exe_path,
        command_line,
        NULL,
        NULL,
        FALSE,
        CREATE_NEW_PROCESS_GROUP,
        NULL,
        install_directory,
        &startup_info,
        &process_info
    )) {
        return false;
    }

    CloseHandle(process_info.hThread);
    CloseHandle(process_info.hProcess);
    return true;
}

static bool ensure_window_class_registered(const wchar_t *class_name, WNDPROC proc) {
    WNDCLASSEXW window_class;
    HINSTANCE instance = GetModuleHandleW(NULL);

    ZeroMemory(&window_class, sizeof(window_class));
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = proc;
    window_class.hInstance = instance;
    window_class.lpszClassName = class_name;
    window_class.hIcon = LoadIconW(NULL, IDI_INFORMATION);
    window_class.hCursor = LoadCursorW(NULL, IDC_ARROW);
    window_class.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    if (!RegisterClassExW(&window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    return true;
}

static void show_notification(const wchar_t *title, const wchar_t *message, bool is_error) {
    NotificationWindowData window_data;
    RECT work_area;
    HWND hwnd;
    MSG message_loop;
    DWORD start_tick = GetTickCount();
    HINSTANCE instance = GetModuleHandleW(NULL);
    int x;
    int y;

    if (!ensure_window_class_registered(BG_NOTIFICATION_CLASS_NAME, notification_window_proc)) {
        MessageBoxW(NULL, message, title, MB_OK | MB_TOPMOST | MB_SETFOREGROUND);
        return;
    }

    if (!SystemParametersInfoW(SPI_GETWORKAREA, 0, &work_area, 0)) {
        work_area.left = 0;
        work_area.top = 0;
        work_area.right = GetSystemMetrics(SM_CXSCREEN);
        work_area.bottom = GetSystemMetrics(SM_CYSCREEN);
    }

    window_data.title = title;
    window_data.message = message;
    if (is_error) {
        window_data.background_color = RGB(62, 18, 24);
        window_data.border_color = RGB(180, 58, 70);
        window_data.title_color = RGB(255, 241, 242);
        window_data.body_color = RGB(255, 221, 225);
    } else {
        window_data.background_color = RGB(28, 33, 43);
        window_data.border_color = RGB(84, 108, 164);
        window_data.title_color = RGB(242, 246, 255);
        window_data.body_color = RGB(214, 223, 244);
    }

    x = work_area.right - BG_NOTIFICATION_WIDTH - BG_NOTIFICATION_MARGIN;
    y = work_area.bottom - BG_NOTIFICATION_HEIGHT - BG_NOTIFICATION_MARGIN;
    hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        BG_NOTIFICATION_CLASS_NAME,
        title,
        WS_POPUP,
        x,
        y,
        BG_NOTIFICATION_WIDTH,
        BG_NOTIFICATION_HEIGHT,
        NULL,
        NULL,
        instance,
        &window_data
    );

    if (hwnd == NULL) {
        MessageBoxW(NULL, message, title, MB_OK | MB_TOPMOST | MB_SETFOREGROUND);
        return;
    }

    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    UpdateWindow(hwnd);
    SetTimer(hwnd, 1, BG_NOTIFICATION_TIMEOUT_MS, NULL);

    while (IsWindow(hwnd)) {
        while (PeekMessageW(&message_loop, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message_loop);
            DispatchMessageW(&message_loop);
        }

        if ((LONG)(GetTickCount() - start_tick) >= (LONG)(BG_NOTIFICATION_TIMEOUT_MS + 250)) {
            DestroyWindow(hwnd);
            break;
        }

        Sleep(25);
    }
}

static ControlMode parse_mode(void) {
    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    ControlMode mode = CONTROL_MODE_TOGGLE;

    if (argv != NULL) {
        for (int i = 1; i < argc; ++i) {
            if (_wcsicmp(argv[i], L"--launch") == 0) {
                mode = CONTROL_MODE_LAUNCH;
                break;
            }
        }
        LocalFree(argv);
    }

    return mode;
}

static int run_launch_mode(const wchar_t *install_directory, const wchar_t *disabled_path) {
    if (is_disabled(disabled_path) || count_guard_processes() > 0) {
        return 0;
    }

    return start_guard_process(install_directory) ? 0 : 1;
}

static int run_toggle_mode(const wchar_t *install_directory, const wchar_t *disabled_path) {
    unsigned int running_count = count_guard_processes();

    if (running_count > 0) {
        write_disabled_flag(disabled_path, true);
        terminate_guard_processes();
        show_notification(L"browser_guard", L"Background protection has been turned off.", false);
        return 0;
    }

    write_disabled_flag(disabled_path, false);
    if (!start_guard_process(install_directory)) {
        show_notification(L"browser_guard", L"browser_guard did not start successfully.", true);
        return 1;
    }

    show_notification(L"browser_guard", L"Background protection is now running.", false);
    return 0;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous_instance, PWSTR command_line, int show_command) {
    wchar_t install_directory[MAX_PATH];
    wchar_t disabled_path[MAX_PATH];
    ControlMode mode;

    (void)instance;
    (void)previous_instance;
    (void)command_line;
    (void)show_command;

    if (!get_module_directory(install_directory, sizeof(install_directory) / sizeof(install_directory[0]))) {
        return 1;
    }
    if (!build_path(disabled_path, MAX_PATH, install_directory, BG_DISABLED_FILE_NAME)) {
        return 1;
    }

    mode = parse_mode();
    if (mode == CONTROL_MODE_LAUNCH) {
        return run_launch_mode(install_directory, disabled_path);
    }

    return run_toggle_mode(install_directory, disabled_path);
}
