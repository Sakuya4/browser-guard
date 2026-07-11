#include <stdint.h>
#include <strsafe.h>
#include <windows.h>

#include "recovery_broker.h"

#ifndef PROCESS_SUSPEND_RESUME
#define PROCESS_SUSPEND_RESUME 0x0800
#endif

typedef LONG NTSTATUS;
typedef NTSTATUS (NTAPI *NtResumeProcessFn)(HANDLE process_handle);

bool bg_start_recovery_broker(const wchar_t *broker_path, const wchar_t *journal_path) {
    SECURITY_ATTRIBUTES attributes;
    HANDLE ready_event = NULL;
    wchar_t command_line[2048];
    STARTUPINFOW startup_info;
    PROCESS_INFORMATION process_info;
    HANDLE wait_handles[2];
    DWORD wait_result;

    if (broker_path == NULL || journal_path == NULL) {
        return false;
    }

    ZeroMemory(&attributes, sizeof(attributes));
    attributes.nLength = sizeof(attributes);
    attributes.bInheritHandle = TRUE;
    ready_event = CreateEventW(&attributes, TRUE, FALSE, NULL);
    if (ready_event == NULL) {
        return false;
    }

    if (FAILED(StringCchPrintfW(
            command_line,
            2048,
            L"\"%ls\" --guard-pid %lu --journal \"%ls\" --ready-handle %llu",
            broker_path,
            GetCurrentProcessId(),
            journal_path,
            (unsigned long long)(uintptr_t)ready_event))) {
        CloseHandle(ready_event);
        return false;
    }

    ZeroMemory(&startup_info, sizeof(startup_info));
    startup_info.cb = sizeof(startup_info);
    ZeroMemory(&process_info, sizeof(process_info));

    if (!CreateProcessW(
            broker_path,
            command_line,
            NULL,
            NULL,
            TRUE,
            CREATE_NO_WINDOW,
            NULL,
            NULL,
            &startup_info,
            &process_info)) {
        CloseHandle(ready_event);
        return false;
    }

    CloseHandle(process_info.hThread);
    wait_handles[0] = ready_event;
    wait_handles[1] = process_info.hProcess;
    wait_result = WaitForMultipleObjects(2, wait_handles, FALSE, 5000);

    CloseHandle(process_info.hProcess);
    CloseHandle(ready_event);
    return wait_result == WAIT_OBJECT_0;
}

bool bg_resume_recovery_record(const BgRecoveryRecord *record) {
    HMODULE ntdll = NULL;
    NtResumeProcessFn resume_process = NULL;
    HANDLE process = NULL;
    NTSTATUS status;

    if (!bg_recovery_record_matches_process(record)) {
        return false;
    }

    ntdll = GetModuleHandleW(L"ntdll.dll");
    if (ntdll == NULL) {
        return false;
    }
    resume_process = (NtResumeProcessFn)GetProcAddress(ntdll, "NtResumeProcess");
    if (resume_process == NULL) {
        return false;
    }

    process = OpenProcess(PROCESS_SUSPEND_RESUME | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, record->pid);
    if (process == NULL) {
        return false;
    }

    status = resume_process(process);
    CloseHandle(process);
    return status >= 0;
}

bool bg_recover_journal_file(const wchar_t *journal_path) {
    BgRecoveryRecord *records = NULL;
    size_t record_count = 0;
    bool all_resolved = true;

    records = (BgRecoveryRecord *)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        BG_MAX_RECOVERY_RECORDS * sizeof(BgRecoveryRecord)
    );
    if (records == NULL) {
        return false;
    }

    if (!bg_recovery_journal_load(
            journal_path,
            records,
            BG_MAX_RECOVERY_RECORDS,
            &record_count)) {
        HeapFree(GetProcessHeap(), 0, records);
        return false;
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
    return all_resolved;
}
