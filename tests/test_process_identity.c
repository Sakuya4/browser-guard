#include <stdbool.h>
#include <stdio.h>
#include <windows.h>

#include "process_identity.h"

int main(void) {
    wchar_t current_path[MAX_PATH];
    DWORD current_pid = GetCurrentProcessId();

    if (GetModuleFileNameW(NULL, current_path, MAX_PATH) == 0) {
        fprintf(stderr, "FAIL: could not resolve test executable path\n");
        return 1;
    }

    if (!bg_process_matches_current_identity(current_pid, current_path)) {
        fprintf(stderr, "FAIL: current process must match its path, user, and session\n");
        return 1;
    }

    if (bg_process_matches_current_identity(current_pid, L"C:\\not-the-current-process.exe")) {
        fprintf(stderr, "FAIL: a process at a different path must not match\n");
        return 1;
    }

    if (bg_process_matches_current_identity(0, current_path)) {
        fprintf(stderr, "FAIL: an invalid PID must fail closed\n");
        return 1;
    }

    return 0;
}
