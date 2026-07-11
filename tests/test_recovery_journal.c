#include <stdio.h>
#include <windows.h>

#include "recovery_journal.h"

int main(void) {
    wchar_t temp_directory[MAX_PATH];
    wchar_t journal_path[MAX_PATH];
    BgRecoveryJournal journal;
    BgRecoveryRecord records[2];
    size_t record_count = 0;

    if (GetTempPathW(MAX_PATH, temp_directory) == 0 ||
        GetTempFileNameW(temp_directory, L"bgr", 0, journal_path) == 0) {
        fprintf(stderr, "FAIL: could not create a temporary journal path\n");
        return 1;
    }
    DeleteFileW(journal_path);

    if (!bg_recovery_journal_init(&journal, journal_path) ||
        !bg_recovery_journal_register(&journal, GetCurrentProcessId())) {
        fprintf(stderr, "FAIL: could not register the current process\n");
        DeleteFileW(journal_path);
        return 1;
    }

    if (!bg_recovery_journal_load(journal_path, records, 2, &record_count) || record_count != 1) {
        fprintf(stderr, "FAIL: persisted journal must contain one record\n");
        DeleteFileW(journal_path);
        return 1;
    }
    if (!bg_recovery_record_matches_process(&records[0])) {
        fprintf(stderr, "FAIL: an unchanged process identity must match\n");
        DeleteFileW(journal_path);
        return 1;
    }

    records[0].creation_time.dwLowDateTime += 1;
    if (bg_recovery_record_matches_process(&records[0])) {
        fprintf(stderr, "FAIL: changed creation time must detect PID reuse\n");
        DeleteFileW(journal_path);
        return 1;
    }

    if (!bg_recovery_journal_unregister(&journal, GetCurrentProcessId()) ||
        GetFileAttributesW(journal_path) != INVALID_FILE_ATTRIBUTES) {
        fprintf(stderr, "FAIL: empty recovery journal must be removed\n");
        DeleteFileW(journal_path);
        return 1;
    }

    return 0;
}
