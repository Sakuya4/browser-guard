#include <strsafe.h>
#include <windows.h>
#include <shellapi.h>

#include "recovery_broker.h"
#include "recovery_journal.h"

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous_instance, PWSTR command_line, int show_command) {
    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    wchar_t fixture_command[1024];
    STARTUPINFOW startup_info;
    PROCESS_INFORMATION process_info;
    BgRecoveryJournal journal;
    bool broker_started;

    (void)instance;
    (void)previous_instance;
    (void)command_line;
    (void)show_command;

    if (argv == NULL || argc != 5 ||
        FAILED(StringCchPrintfW(fixture_command, 1024, L"\"%ls\" \"%ls\"", argv[2], argv[4]))) {
        if (argv != NULL) {
            LocalFree(argv);
        }
        return 1;
    }

    ZeroMemory(&startup_info, sizeof(startup_info));
    startup_info.cb = sizeof(startup_info);
    ZeroMemory(&process_info, sizeof(process_info));

    if (!bg_recovery_journal_init(&journal, argv[3]) ||
        !CreateProcessW(
            argv[2],
            fixture_command,
            NULL,
            NULL,
            FALSE,
            CREATE_SUSPENDED | CREATE_NO_WINDOW,
            NULL,
            NULL,
            &startup_info,
            &process_info)) {
        LocalFree(argv);
        return 1;
    }

    if (!bg_recovery_journal_register(&journal, process_info.dwProcessId)) {
        TerminateProcess(process_info.hProcess, 1);
        CloseHandle(process_info.hThread);
        CloseHandle(process_info.hProcess);
        LocalFree(argv);
        return 1;
    }

    broker_started = bg_start_recovery_broker(argv[1], argv[3]);
    if (!broker_started) {
        ResumeThread(process_info.hThread);
        bg_recovery_journal_unregister(&journal, process_info.dwProcessId);
    }

    CloseHandle(process_info.hThread);
    CloseHandle(process_info.hProcess);
    LocalFree(argv);
    return broker_started ? 0 : 1;
}
