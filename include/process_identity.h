#ifndef PROCESS_IDENTITY_H
#define PROCESS_IDENTITY_H

#include <stdbool.h>
#include <windows.h>

bool bg_process_matches_current_identity(DWORD pid, const wchar_t *expected_path);

#endif
