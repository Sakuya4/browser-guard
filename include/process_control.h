#ifndef PROCESS_CONTROL_H
#define PROCESS_CONTROL_H

#include <stdbool.h>
#include <stddef.h>
#include <windows.h>

#include "app_config.h"

#define BG_MAX_BROWSER_GROUPS 16
#define BG_MAX_GROUP_PIDS 256
#define BG_MAX_TRACKED_PROCESSES 512

typedef struct BrowserGroup {
    wchar_t exe_name[MAX_PATH];
    DWORD pids[BG_MAX_GROUP_PIDS];
    size_t pid_count;
    bool has_foreground_window;
    bool has_audio;
} BrowserGroup;

typedef struct SecurityContext {
    DWORD current_session_id;
    BYTE current_user_sid[SECURITY_MAX_SID_SIZE];
    DWORD current_user_sid_size;
} SecurityContext;

typedef struct MemoryTotals {
    SIZE_T working_set_bytes;
    SIZE_T private_bytes;
    size_t process_count;
} MemoryTotals;

bool init_security_context(SecurityContext *context);
void collect_browser_groups(const SecurityContext *context, BrowserGroup *groups, size_t *group_count, size_t capacity);
void mark_foreground_groups(BrowserGroup *groups, size_t group_count);
void mark_audio_groups(BrowserGroup *groups, size_t group_count);
bool set_process_suspended(DWORD pid, bool suspend, const AppConfig *config);
bool maintain_suspended_process(DWORD pid, const AppConfig *config);
bool snapshot_memory_totals(const BrowserGroup *groups, size_t group_count, MemoryTotals *totals);

#endif
