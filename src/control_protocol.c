#include <stdbool.h>
#include <stddef.h>
#include <windows.h>
#include <sddl.h>
#include <strsafe.h>

#include "control_protocol.h"

#define BG_SHUTDOWN_EVENT_NAME_CAPACITY 96
#define BG_SECURITY_DESCRIPTOR_CAPACITY 256

static bool build_shutdown_event_name(wchar_t *buffer, size_t buffer_count) {
    DWORD session_id = 0;

    if (!ProcessIdToSessionId(GetCurrentProcessId(), &session_id)) {
        return false;
    }

    return SUCCEEDED(StringCchPrintfW(
        buffer,
        buffer_count,
        L"Local\\BrowserGuard.Shutdown.%lu",
        session_id
    ));
}

static bool build_event_security_attributes(
    SECURITY_ATTRIBUTES *attributes,
    PSECURITY_DESCRIPTOR *security_descriptor
) {
    HANDLE token = NULL;
    BYTE token_buffer[sizeof(TOKEN_USER) + SECURITY_MAX_SID_SIZE];
    DWORD token_size = 0;
    TOKEN_USER *token_user = (TOKEN_USER *)token_buffer;
    LPWSTR sid_string = NULL;
    wchar_t sddl[BG_SECURITY_DESCRIPTOR_CAPACITY];
    bool success = false;

    ZeroMemory(attributes, sizeof(*attributes));
    *security_descriptor = NULL;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        return false;
    }

    if (!GetTokenInformation(token, TokenUser, token_buffer, sizeof(token_buffer), &token_size)) {
        goto cleanup;
    }
    if (!ConvertSidToStringSidW(token_user->User.Sid, &sid_string)) {
        goto cleanup;
    }
    if (FAILED(StringCchPrintfW(sddl, BG_SECURITY_DESCRIPTOR_CAPACITY, L"D:P(A;;GA;;;%ls)", sid_string))) {
        goto cleanup;
    }
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
            sddl,
            SDDL_REVISION_1,
            security_descriptor,
            NULL)) {
        goto cleanup;
    }

    attributes->nLength = sizeof(*attributes);
    attributes->lpSecurityDescriptor = *security_descriptor;
    attributes->bInheritHandle = FALSE;
    success = true;

cleanup:
    if (sid_string != NULL) {
        LocalFree(sid_string);
    }
    CloseHandle(token);
    return success;
}

HANDLE bg_create_shutdown_event(void) {
    wchar_t event_name[BG_SHUTDOWN_EVENT_NAME_CAPACITY];
    SECURITY_ATTRIBUTES attributes;
    PSECURITY_DESCRIPTOR security_descriptor = NULL;
    HANDLE shutdown_event = NULL;

    if (!build_shutdown_event_name(event_name, BG_SHUTDOWN_EVENT_NAME_CAPACITY) ||
        !build_event_security_attributes(&attributes, &security_descriptor)) {
        return NULL;
    }

    shutdown_event = CreateEventW(&attributes, TRUE, FALSE, event_name);
    LocalFree(security_descriptor);

    if (shutdown_event != NULL && GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(shutdown_event);
        SetLastError(ERROR_ALREADY_EXISTS);
        return NULL;
    }

    return shutdown_event;
}

HANDLE bg_open_shutdown_event(void) {
    wchar_t event_name[BG_SHUTDOWN_EVENT_NAME_CAPACITY];

    if (!build_shutdown_event_name(event_name, BG_SHUTDOWN_EVENT_NAME_CAPACITY)) {
        return NULL;
    }

    return OpenEventW(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, event_name);
}
