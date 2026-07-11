#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>

#include "process_identity.h"
#include "recovery_journal.h"

#define BG_RECOVERY_MAGIC 0x4252474Au
#define BG_RECOVERY_VERSION 1u

typedef struct BgRecoveryFileHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t record_count;
} BgRecoveryFileHeader;

static bool write_all(HANDLE file, const void *data, DWORD byte_count) {
    DWORD bytes_written = 0;
    return WriteFile(file, data, byte_count, &bytes_written, NULL) && bytes_written == byte_count;
}

static bool read_all(HANDLE file, void *data, DWORD byte_count) {
    DWORD bytes_read = 0;
    return ReadFile(file, data, byte_count, &bytes_read, NULL) && bytes_read == byte_count;
}

static bool persist_journal(const BgRecoveryJournal *journal) {
    wchar_t temporary_path[MAX_PATH];
    HANDLE file = INVALID_HANDLE_VALUE;
    BgRecoveryFileHeader header;
    bool success;

    if (journal->record_count == 0) {
        DeleteFileW(journal->path);
        return true;
    }
    if (swprintf_s(temporary_path, MAX_PATH, L"%ls.tmp", journal->path) < 0) {
        return false;
    }

    file = CreateFileW(
        temporary_path,
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_TEMPORARY,
        NULL
    );
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    header.magic = BG_RECOVERY_MAGIC;
    header.version = BG_RECOVERY_VERSION;
    header.record_count = (uint32_t)journal->record_count;
    success = write_all(file, &header, sizeof(header)) &&
              write_all(file, journal->records, (DWORD)(journal->record_count * sizeof(journal->records[0]))) &&
              FlushFileBuffers(file);
    CloseHandle(file);

    if (!success || !MoveFileExW(
            temporary_path,
            journal->path,
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(temporary_path);
        return false;
    }

    return true;
}

static bool query_recovery_record(DWORD pid, BgRecoveryRecord *record) {
    HANDLE process = NULL;
    FILETIME exit_time;
    FILETIME kernel_time;
    FILETIME user_time;
    DWORD path_size = MAX_PATH;

    ZeroMemory(record, sizeof(*record));
    process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (process == NULL) {
        return false;
    }

    record->pid = pid;
    if (!GetProcessTimes(process, &record->creation_time, &exit_time, &kernel_time, &user_time) ||
        !ProcessIdToSessionId(pid, &record->session_id) ||
        !QueryFullProcessImageNameW(process, 0, record->executable_path, &path_size)) {
        CloseHandle(process);
        return false;
    }

    CloseHandle(process);
    return bg_process_matches_current_identity(pid, record->executable_path);
}

bool bg_recovery_journal_init(BgRecoveryJournal *journal, const wchar_t *path) {
    size_t path_length;

    if (journal == NULL || path == NULL) {
        return false;
    }

    path_length = wcslen(path);
    if (path_length == 0 || path_length >= MAX_PATH) {
        return false;
    }

    ZeroMemory(journal, sizeof(*journal));
    wcscpy_s(journal->path, MAX_PATH, path);
    DeleteFileW(journal->path);
    return true;
}

bool bg_recovery_journal_register(BgRecoveryJournal *journal, DWORD pid) {
    BgRecoveryRecord record;

    if (journal == NULL || pid == 0) {
        return false;
    }
    for (size_t i = 0; i < journal->record_count; ++i) {
        if (journal->records[i].pid == pid) {
            return true;
        }
    }
    if (journal->record_count >= BG_MAX_RECOVERY_RECORDS || !query_recovery_record(pid, &record)) {
        return false;
    }

    journal->records[journal->record_count++] = record;
    if (!persist_journal(journal)) {
        journal->record_count -= 1;
        ZeroMemory(&journal->records[journal->record_count], sizeof(record));
        return false;
    }

    return true;
}

bool bg_recovery_journal_unregister(BgRecoveryJournal *journal, DWORD pid) {
    size_t index;

    if (journal == NULL || pid == 0) {
        return false;
    }
    for (index = 0; index < journal->record_count; ++index) {
        if (journal->records[index].pid == pid) {
            break;
        }
    }
    if (index == journal->record_count) {
        return true;
    }

    if (index + 1 < journal->record_count) {
        memmove(
            &journal->records[index],
            &journal->records[index + 1],
            (journal->record_count - index - 1) * sizeof(journal->records[0])
        );
    }
    journal->record_count -= 1;
    ZeroMemory(&journal->records[journal->record_count], sizeof(journal->records[0]));
    return persist_journal(journal);
}

bool bg_recovery_journal_load(
    const wchar_t *path,
    BgRecoveryRecord *records,
    size_t capacity,
    size_t *record_count
) {
    HANDLE file;
    BgRecoveryFileHeader header;
    DWORD error;

    if (path == NULL || records == NULL || record_count == NULL) {
        return false;
    }
    *record_count = 0;

    file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        error = GetLastError();
        return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
    }

    if (!read_all(file, &header, sizeof(header)) ||
        header.magic != BG_RECOVERY_MAGIC ||
        header.version != BG_RECOVERY_VERSION ||
        header.record_count > capacity ||
        header.record_count > BG_MAX_RECOVERY_RECORDS ||
        !read_all(file, records, header.record_count * (DWORD)sizeof(records[0]))) {
        CloseHandle(file);
        return false;
    }

    CloseHandle(file);
    *record_count = header.record_count;
    return true;
}

bool bg_recovery_record_matches_process(const BgRecoveryRecord *record) {
    HANDLE process = NULL;
    FILETIME creation_time;
    FILETIME exit_time;
    FILETIME kernel_time;
    FILETIME user_time;
    DWORD session_id = 0;
    bool matches = false;

    if (record == NULL || record->pid == 0 ||
        !bg_process_matches_current_identity(record->pid, record->executable_path)) {
        return false;
    }

    process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, record->pid);
    if (process == NULL) {
        return false;
    }

    if (GetProcessTimes(process, &creation_time, &exit_time, &kernel_time, &user_time) &&
        ProcessIdToSessionId(record->pid, &session_id) &&
        session_id == record->session_id &&
        CompareFileTime(&creation_time, &record->creation_time) == 0) {
        matches = true;
    }

    CloseHandle(process);
    return matches;
}
