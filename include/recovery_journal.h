#ifndef RECOVERY_JOURNAL_H
#define RECOVERY_JOURNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <windows.h>

#define BG_MAX_RECOVERY_RECORDS 512

typedef struct BgRecoveryRecord {
    DWORD pid;
    FILETIME creation_time;
    DWORD session_id;
    wchar_t executable_path[MAX_PATH];
} BgRecoveryRecord;

typedef struct BgRecoveryJournal {
    wchar_t path[MAX_PATH];
    BgRecoveryRecord records[BG_MAX_RECOVERY_RECORDS];
    size_t record_count;
} BgRecoveryJournal;

bool bg_recovery_journal_init(BgRecoveryJournal *journal, const wchar_t *path);
bool bg_recovery_journal_register(BgRecoveryJournal *journal, DWORD pid);
bool bg_recovery_journal_unregister(BgRecoveryJournal *journal, DWORD pid);
bool bg_recovery_journal_load(
    const wchar_t *path,
    BgRecoveryRecord *records,
    size_t capacity,
    size_t *record_count
);
bool bg_recovery_record_matches_process(const BgRecoveryRecord *record);

#endif
