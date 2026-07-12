#include <stdbool.h>
#include <wchar.h>
#include <windows.h>

#include "process_identity.h"

#define BG_IDENTITY_PATH_CAPACITY 32768

static bool query_process_user_sid(HANDLE process, BYTE *buffer, DWORD buffer_size, PSID *sid) {
    HANDLE token = NULL;
    DWORD required_size = 0;
    TOKEN_USER *token_user = (TOKEN_USER *)buffer;
    bool success = false;

    if (!OpenProcessToken(process, TOKEN_QUERY, &token)) {
        return false;
    }

    if (GetTokenInformation(token, TokenUser, buffer, buffer_size, &required_size)) {
        *sid = token_user->User.Sid;
        success = true;
    }

    CloseHandle(token);
    return success;
}

bool bg_process_matches_current_identity(DWORD pid, const wchar_t *expected_path) {
    HANDLE process = NULL;
    wchar_t actual_path[BG_IDENTITY_PATH_CAPACITY];
    DWORD actual_path_size = BG_IDENTITY_PATH_CAPACITY;
    DWORD current_session_id = 0;
    DWORD target_session_id = 0;
    BYTE current_user_buffer[sizeof(TOKEN_USER) + SECURITY_MAX_SID_SIZE];
    BYTE target_user_buffer[sizeof(TOKEN_USER) + SECURITY_MAX_SID_SIZE];
    PSID current_user_sid = NULL;
    PSID target_user_sid = NULL;
    bool matches = false;

    if (pid == 0 || expected_path == NULL || expected_path[0] == L'\0') {
        return false;
    }

    process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (process == NULL) {
        return false;
    }

    if (!QueryFullProcessImageNameW(process, 0, actual_path, &actual_path_size) ||
        _wcsicmp(actual_path, expected_path) != 0) {
        goto cleanup;
    }

    if (!ProcessIdToSessionId(GetCurrentProcessId(), &current_session_id) ||
        !ProcessIdToSessionId(pid, &target_session_id) ||
        current_session_id != target_session_id) {
        goto cleanup;
    }

    if (!query_process_user_sid(
            GetCurrentProcess(),
            current_user_buffer,
            sizeof(current_user_buffer),
            &current_user_sid) ||
        !query_process_user_sid(
            process,
            target_user_buffer,
            sizeof(target_user_buffer),
            &target_user_sid) ||
        !EqualSid(current_user_sid, target_user_sid)) {
        goto cleanup;
    }

    matches = true;

cleanup:
    CloseHandle(process);
    return matches;
}
