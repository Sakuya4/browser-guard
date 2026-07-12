#include <stdint.h>
#include <stdlib.h>
#include <wchar.h>
#include <windows.h>
#include <shellapi.h>

#include "recovery_broker.h"

static int run_recovery_broker(DWORD guard_pid, const wchar_t *journal_path, HANDLE ready_event) {
    HANDLE guard_process = NULL;

    guard_process = OpenProcess(SYNCHRONIZE, FALSE, guard_pid);
    if (guard_process == NULL) {
        return 1;
    }

    SetEvent(ready_event);
    WaitForSingleObject(guard_process, INFINITE);
    CloseHandle(guard_process);
    return bg_recover_journal_file(journal_path) ? 0 : 1;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous_instance, PWSTR command_line, int show_command) {
    int argc = 0;
    LPWSTR *argv = NULL;
    DWORD guard_pid = 0;
    const wchar_t *journal_path = NULL;
    HANDLE ready_event = NULL;
    int result = 1;

    (void)instance;
    (void)previous_instance;
    (void)command_line;
    (void)show_command;

    argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv == NULL) {
        return 1;
    }

    for (int i = 1; i + 1 < argc; ++i) {
        if (_wcsicmp(argv[i], L"--guard-pid") == 0) {
            guard_pid = wcstoul(argv[++i], NULL, 10);
        } else if (_wcsicmp(argv[i], L"--journal") == 0) {
            journal_path = argv[++i];
        } else if (_wcsicmp(argv[i], L"--ready-handle") == 0) {
            ready_event = (HANDLE)(uintptr_t)_wcstoui64(argv[++i], NULL, 10);
        }
    }

    if (guard_pid != 0 && journal_path != NULL && ready_event != NULL) {
        result = run_recovery_broker(guard_pid, journal_path, ready_event);
    }

    LocalFree(argv);
    return result;
}
