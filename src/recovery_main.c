#include <stdint.h>
#include <stdlib.h>
#include <wchar.h>
#include <windows.h>
#include <shellapi.h>

#include "recovery_broker.h"

static int run_recovery_broker(DWORD guard_pid, const wchar_t *journal_path, HANDLE ready_event) {
    HANDLE guard_process = NULL;
    BgRecoveryRecord *records = NULL;
    size_t record_count = 0;
    bool all_resolved = true;

    guard_process = OpenProcess(SYNCHRONIZE, FALSE, guard_pid);
    if (guard_process == NULL) {
        SetEvent(ready_event);
        return 1;
    }

    SetEvent(ready_event);
    WaitForSingleObject(guard_process, INFINITE);
    CloseHandle(guard_process);

    records = (BgRecoveryRecord *)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        BG_MAX_RECOVERY_RECORDS * sizeof(BgRecoveryRecord)
    );
    if (records == NULL) {
        return 1;
    }

    if (!bg_recovery_journal_load(
            journal_path,
            records,
            BG_MAX_RECOVERY_RECORDS,
            &record_count)) {
        HeapFree(GetProcessHeap(), 0, records);
        return 1;
    }

    for (size_t i = 0; i < record_count; ++i) {
        bool resolved = !bg_recovery_record_matches_process(&records[i]);

        for (unsigned int attempt = 0; !resolved && attempt < 20; ++attempt) {
            resolved = bg_resume_recovery_record(&records[i]);
            if (!resolved) {
                Sleep(100);
            }
        }
        if (!resolved) {
            all_resolved = false;
        }
    }

    HeapFree(GetProcessHeap(), 0, records);
    if (all_resolved) {
        DeleteFileW(journal_path);
    }
    return all_resolved ? 0 : 1;
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
