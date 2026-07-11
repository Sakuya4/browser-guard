#ifndef RECOVERY_BROKER_H
#define RECOVERY_BROKER_H

#include <stdbool.h>
#include <windows.h>

#include "recovery_journal.h"

bool bg_start_recovery_broker(const wchar_t *broker_path, const wchar_t *journal_path);
bool bg_resume_recovery_record(const BgRecoveryRecord *record);
bool bg_recover_journal_file(const wchar_t *journal_path);

#endif
