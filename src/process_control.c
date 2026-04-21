#define COBJMACROS
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <initguid.h>
#include <stdio.h>
#include <wchar.h>

#include "process_control.h"

#ifndef PROCESS_SUSPEND_RESUME
#define PROCESS_SUSPEND_RESUME 0x0800
#endif

typedef LONG NTSTATUS;
typedef NTSTATUS (NTAPI *NtSuspendProcessFn)(HANDLE process_handle);
typedef NTSTATUS (NTAPI *NtResumeProcessFn)(HANDLE process_handle);

DEFINE_GUID(IID_IMMDeviceEnumerator, 0xA95664D2, 0x9614, 0x4F35, 0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6);
DEFINE_GUID(CLSID_MMDeviceEnumerator, 0xBCDE0395, 0xE52F, 0x467C, 0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E);
DEFINE_GUID(IID_IAudioSessionControl2, 0xBFB7FF88, 0x7239, 0x4FC9, 0x8F, 0xA2, 0x07, 0xC9, 0x50, 0xBE, 0x9C, 0x6D);
DEFINE_GUID(IID_IAudioSessionManager2, 0x77AA99A0, 0x1BD6, 0x484F, 0x8B, 0xC7, 0x2C, 0x65, 0x4C, 0x9A, 0x9B, 0x6F);

static const wchar_t *k_supported_browsers[] = {
    L"chrome.exe",
    L"msedge.exe",
    L"firefox.exe",
    L"brave.exe",
    L"opera.exe",
    L"vivaldi.exe",
};

typedef struct WindowScanContext {
    BrowserGroup *groups;
    size_t group_count;
    HWND foreground_window;
} WindowScanContext;

static bool is_supported_browser_name(const wchar_t *exe_name) {
    for (size_t i = 0; i < sizeof(k_supported_browsers) / sizeof(k_supported_browsers[0]); ++i) {
        if (_wcsicmp(exe_name, k_supported_browsers[i]) == 0) {
            return true;
        }
    }

    return false;
}

static bool pid_exists_in_array(const DWORD *pids, size_t count, DWORD pid) {
    for (size_t i = 0; i < count; ++i) {
        if (pids[i] == pid) {
            return true;
        }
    }

    return false;
}

static bool copy_current_user_sid(SecurityContext *context, HANDLE token) {
    BYTE stack_buffer[256];
    BYTE *token_buffer = stack_buffer;
    DWORD required_size = sizeof(stack_buffer);

    if (!GetTokenInformation(token, TokenUser, token_buffer, required_size, &required_size)) {
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            return false;
        }

        token_buffer = (BYTE *)HeapAlloc(GetProcessHeap(), 0, required_size);
        if (token_buffer == NULL) {
            return false;
        }

        if (!GetTokenInformation(token, TokenUser, token_buffer, required_size, &required_size)) {
            HeapFree(GetProcessHeap(), 0, token_buffer);
            return false;
        }
    }

    TOKEN_USER *token_user = (TOKEN_USER *)token_buffer;
    DWORD sid_size = GetLengthSid(token_user->User.Sid);
    if (sid_size > sizeof(context->current_user_sid)) {
        if (token_buffer != stack_buffer) {
            HeapFree(GetProcessHeap(), 0, token_buffer);
        }
        return false;
    }

    if (!CopySid(sizeof(context->current_user_sid), context->current_user_sid, token_user->User.Sid)) {
        if (token_buffer != stack_buffer) {
            HeapFree(GetProcessHeap(), 0, token_buffer);
        }
        return false;
    }

    context->current_user_sid_size = sid_size;

    if (token_buffer != stack_buffer) {
        HeapFree(GetProcessHeap(), 0, token_buffer);
    }

    return true;
}

bool init_security_context(SecurityContext *context) {
    HANDLE token = NULL;

    ZeroMemory(context, sizeof(*context));
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &context->current_session_id)) {
        return false;
    }

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        return false;
    }

    bool success = copy_current_user_sid(context, token);
    CloseHandle(token);
    return success;
}

static bool query_process_base_name(DWORD pid, wchar_t *buffer, size_t buffer_len) {
    bool success = false;
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (process == NULL) {
        return false;
    }

    DWORD copied = (DWORD)buffer_len;
    if (QueryFullProcessImageNameW(process, 0, buffer, &copied)) {
        wchar_t *last_slash = wcsrchr(buffer, L'\\');
        if (last_slash != NULL) {
            memmove(buffer, last_slash + 1, (wcslen(last_slash + 1) + 1) * sizeof(wchar_t));
        }
        success = true;
    }

    CloseHandle(process);
    return success;
}

static bool process_matches_current_user(const SecurityContext *context, HANDLE process) {
    HANDLE token = NULL;
    BYTE stack_buffer[256];
    BYTE *token_buffer = stack_buffer;
    DWORD required_size = sizeof(stack_buffer);
    bool matches = false;

    if (!OpenProcessToken(process, TOKEN_QUERY, &token)) {
        return false;
    }

    if (!GetTokenInformation(token, TokenUser, token_buffer, required_size, &required_size)) {
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            CloseHandle(token);
            return false;
        }

        token_buffer = (BYTE *)HeapAlloc(GetProcessHeap(), 0, required_size);
        if (token_buffer == NULL) {
            CloseHandle(token);
            return false;
        }

        if (!GetTokenInformation(token, TokenUser, token_buffer, required_size, &required_size)) {
            HeapFree(GetProcessHeap(), 0, token_buffer);
            CloseHandle(token);
            return false;
        }
    }

    TOKEN_USER *token_user = (TOKEN_USER *)token_buffer;
    matches = EqualSid((PSID)context->current_user_sid, token_user->User.Sid) != FALSE;

    if (token_buffer != stack_buffer) {
        HeapFree(GetProcessHeap(), 0, token_buffer);
    }

    CloseHandle(token);
    return matches;
}

static bool is_manageable_browser_process(const SecurityContext *context, DWORD pid, const wchar_t *exe_name) {
    DWORD session_id = 0;
    if (!is_supported_browser_name(exe_name)) {
        return false;
    }

    if (!ProcessIdToSessionId(pid, &session_id) || session_id != context->current_session_id) {
        return false;
    }

    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (process == NULL) {
        return false;
    }

    bool matches = process_matches_current_user(context, process);
    CloseHandle(process);
    return matches;
}

bool is_manageable_browser_pid(const SecurityContext *context, DWORD pid) {
    wchar_t exe_name[MAX_PATH];

    ZeroMemory(exe_name, sizeof(exe_name));
    if (!query_process_base_name(pid, exe_name, MAX_PATH)) {
        return false;
    }

    return is_manageable_browser_process(context, pid, exe_name);
}

static BrowserGroup *find_group_by_name(BrowserGroup *groups, size_t group_count, const wchar_t *exe_name) {
    for (size_t i = 0; i < group_count; ++i) {
        if (_wcsicmp(groups[i].exe_name, exe_name) == 0) {
            return &groups[i];
        }
    }

    return NULL;
}

static BrowserGroup *get_or_add_group(BrowserGroup *groups, size_t *group_count, size_t capacity, const wchar_t *exe_name) {
    BrowserGroup *existing = find_group_by_name(groups, *group_count, exe_name);
    if (existing != NULL) {
        return existing;
    }

    if (*group_count >= capacity) {
        return NULL;
    }

    BrowserGroup *group = &groups[*group_count];
    ZeroMemory(group, sizeof(*group));
    wcsncpy(group->exe_name, exe_name, MAX_PATH - 1);
    *group_count += 1;
    return group;
}

void collect_browser_groups(const SecurityContext *context, BrowserGroup *groups, size_t *group_count, size_t capacity) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return;
    }

    PROCESSENTRY32W entry;
    ZeroMemory(&entry, sizeof(entry));
    entry.dwSize = sizeof(entry);

    if (!Process32FirstW(snapshot, &entry)) {
        CloseHandle(snapshot);
        return;
    }

    do {
        if (!is_manageable_browser_process(context, entry.th32ProcessID, entry.szExeFile)) {
            continue;
        }

        BrowserGroup *group = get_or_add_group(groups, group_count, capacity, entry.szExeFile);
        if (group == NULL || group->pid_count >= BG_MAX_GROUP_PIDS) {
            continue;
        }

        group->pids[group->pid_count++] = entry.th32ProcessID;
    } while (Process32NextW(snapshot, &entry));

    CloseHandle(snapshot);
}

static BOOL CALLBACK enum_windows_callback(HWND hwnd, LPARAM lparam) {
    WindowScanContext *context = (WindowScanContext *)lparam;
    if (!IsWindowVisible(hwnd)) {
        return TRUE;
    }

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0) {
        return TRUE;
    }

    wchar_t exe_name[MAX_PATH];
    ZeroMemory(exe_name, sizeof(exe_name));
    if (!query_process_base_name(pid, exe_name, MAX_PATH)) {
        return TRUE;
    }

    BrowserGroup *group = find_group_by_name(context->groups, context->group_count, exe_name);
    if (group == NULL) {
        return TRUE;
    }

    group->has_visible_window = true;
    if (group->anchor_window == NULL && !IsIconic(hwnd)) {
        group->anchor_window = GetAncestor(hwnd, GA_ROOT);
    }

    if (IsIconic(hwnd)) {
        group->is_minimized = true;
    }

    if (hwnd == context->foreground_window && !IsIconic(hwnd)) {
        group->has_foreground_window = true;
        group->anchor_window = GetAncestor(hwnd, GA_ROOT);
        group->is_minimized = false;
    }

    return TRUE;
}

void mark_foreground_groups(BrowserGroup *groups, size_t group_count) {
    WindowScanContext context = {
        .groups = groups,
        .group_count = group_count,
        .foreground_window = GetForegroundWindow(),
    };

    EnumWindows(enum_windows_callback, (LPARAM)&context);
}

static void collect_active_audio_pids(DWORD *audio_pids, size_t *audio_pid_count, size_t capacity) {
    IMMDeviceEnumerator *device_enumerator = NULL;
    IMMDevice *default_device = NULL;
    IAudioSessionManager2 *session_manager = NULL;
    IAudioSessionEnumerator *session_enumerator = NULL;
    HRESULT hr = S_OK;

    *audio_pid_count = 0;
    hr = CoCreateInstance(
        &CLSID_MMDeviceEnumerator,
        NULL,
        CLSCTX_ALL,
        &IID_IMMDeviceEnumerator,
        (void **)&device_enumerator
    );
    if (FAILED(hr)) {
        goto cleanup;
    }

    hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(device_enumerator, eRender, eMultimedia, &default_device);
    if (FAILED(hr)) {
        goto cleanup;
    }

    hr = IMMDevice_Activate(default_device, &IID_IAudioSessionManager2, CLSCTX_ALL, NULL, (void **)&session_manager);
    if (FAILED(hr)) {
        goto cleanup;
    }

    hr = IAudioSessionManager2_GetSessionEnumerator(session_manager, &session_enumerator);
    if (FAILED(hr)) {
        goto cleanup;
    }

    int session_count = 0;
    hr = IAudioSessionEnumerator_GetCount(session_enumerator, &session_count);
    if (FAILED(hr)) {
        goto cleanup;
    }

    for (int i = 0; i < session_count; ++i) {
        IAudioSessionControl *session_control = NULL;
        IAudioSessionControl2 *session_control2 = NULL;
        AudioSessionState session_state = AudioSessionStateInactive;
        DWORD pid = 0;

        hr = IAudioSessionEnumerator_GetSession(session_enumerator, i, &session_control);
        if (FAILED(hr) || session_control == NULL) {
            continue;
        }

        hr = IAudioSessionControl_QueryInterface(session_control, &IID_IAudioSessionControl2, (void **)&session_control2);
        if (FAILED(hr) || session_control2 == NULL) {
            IAudioSessionControl_Release(session_control);
            continue;
        }

        if (SUCCEEDED(IAudioSessionControl_GetState(session_control, &session_state)) &&
            SUCCEEDED(IAudioSessionControl2_GetProcessId(session_control2, &pid)) &&
            session_state == AudioSessionStateActive &&
            pid != 0 &&
            *audio_pid_count < capacity &&
            !pid_exists_in_array(audio_pids, *audio_pid_count, pid)) {
            audio_pids[(*audio_pid_count)++] = pid;
        }

        IAudioSessionControl2_Release(session_control2);
        IAudioSessionControl_Release(session_control);
    }

cleanup:
    if (session_enumerator != NULL) {
        IAudioSessionEnumerator_Release(session_enumerator);
    }
    if (session_manager != NULL) {
        IAudioSessionManager2_Release(session_manager);
    }
    if (default_device != NULL) {
        IMMDevice_Release(default_device);
    }
    if (device_enumerator != NULL) {
        IMMDeviceEnumerator_Release(device_enumerator);
    }
}

void mark_audio_groups(BrowserGroup *groups, size_t group_count) {
    DWORD audio_pids[BG_MAX_TRACKED_PROCESSES];
    size_t audio_pid_count = 0;

    collect_active_audio_pids(audio_pids, &audio_pid_count, BG_MAX_TRACKED_PROCESSES);
    for (size_t i = 0; i < group_count; ++i) {
        for (size_t j = 0; j < groups[i].pid_count; ++j) {
            if (pid_exists_in_array(audio_pids, audio_pid_count, groups[i].pids[j])) {
                groups[i].has_audio = true;
                break;
            }
        }
    }
}

static void apply_memory_priority(HANDLE process, bool lower_memory_priority) {
    MEMORY_PRIORITY_INFORMATION info;

    ZeroMemory(&info, sizeof(info));
    info.MemoryPriority = lower_memory_priority ? MEMORY_PRIORITY_VERY_LOW : MEMORY_PRIORITY_NORMAL;
    SetProcessInformation(process, ProcessMemoryPriority, &info, sizeof(info));
}

static void apply_power_throttling(HANDLE process, bool enable_power_throttling) {
    PROCESS_POWER_THROTTLING_STATE state;
    DWORD flags = PROCESS_POWER_THROTTLING_EXECUTION_SPEED | PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION;

    ZeroMemory(&state, sizeof(state));
    state.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
    state.ControlMask = flags;
    state.StateMask = enable_power_throttling ? flags : 0;
    SetProcessInformation(process, ProcessPowerThrottling, &state, sizeof(state));
}

static bool open_process_for_management(DWORD pid, bool needs_quota, HANDLE *out_process) {
    DWORD access_mask = PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_SUSPEND_RESUME | PROCESS_SET_INFORMATION;
    if (needs_quota) {
        access_mask |= PROCESS_SET_QUOTA;
    }

    *out_process = OpenProcess(access_mask, FALSE, pid);
    return *out_process != NULL;
}

static bool invoke_suspend_or_resume(HANDLE process, bool suspend) {
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (ntdll == NULL) {
        return false;
    }

    NtSuspendProcessFn nt_suspend = (NtSuspendProcessFn)GetProcAddress(ntdll, "NtSuspendProcess");
    NtResumeProcessFn nt_resume = (NtResumeProcessFn)GetProcAddress(ntdll, "NtResumeProcess");
    if (nt_suspend == NULL || nt_resume == NULL) {
        return false;
    }

    NTSTATUS status = suspend ? nt_suspend(process) : nt_resume(process);
    return status >= 0;
}

bool set_process_background_mode(DWORD pid, const AppConfig *config) {
    HANDLE process = NULL;
    bool needs_quota = config->trim_working_set;

    if (!open_process_for_management(pid, needs_quota, &process)) {
        return false;
    }

    if (config->lower_memory_priority) {
        apply_memory_priority(process, true);
    }
    if (config->enable_power_throttling) {
        apply_power_throttling(process, true);
    }
    if (config->trim_working_set) {
        EmptyWorkingSet(process);
    }

    CloseHandle(process);
    return true;
}

bool restore_process_foreground_mode(DWORD pid, const AppConfig *config) {
    HANDLE process = NULL;

    if (!open_process_for_management(pid, false, &process)) {
        return false;
    }

    if (config->lower_memory_priority) {
        apply_memory_priority(process, false);
    }
    if (config->enable_power_throttling) {
        apply_power_throttling(process, false);
    }

    CloseHandle(process);
    return true;
}

bool set_process_suspended(DWORD pid, bool suspend, const AppConfig *config) {
    HANDLE process = NULL;
    bool success = false;
    bool needs_quota = config->trim_working_set;

    if (!open_process_for_management(pid, needs_quota, &process)) {
        return false;
    }

    if (suspend) {
        if (config->lower_memory_priority) {
            apply_memory_priority(process, true);
        }
        if (config->enable_power_throttling) {
            apply_power_throttling(process, true);
        }
    }

    success = invoke_suspend_or_resume(process, suspend);
    if (!success) {
        if (suspend && config->lower_memory_priority) {
            apply_memory_priority(process, false);
        }
        if (suspend && config->enable_power_throttling) {
            apply_power_throttling(process, false);
        }
        CloseHandle(process);
        return false;
    }

    if (suspend && config->trim_working_set) {
        EmptyWorkingSet(process);
    }

    if (!suspend) {
        if (config->lower_memory_priority) {
            apply_memory_priority(process, false);
        }
        if (config->enable_power_throttling) {
            apply_power_throttling(process, false);
        }
    }

    CloseHandle(process);
    return true;
}

bool maintain_suspended_process(DWORD pid, const AppConfig *config) {
    HANDLE process = NULL;
    if (!config->trim_working_set) {
        return true;
    }

    if (!open_process_for_management(pid, true, &process)) {
        return false;
    }

    if (config->lower_memory_priority) {
        apply_memory_priority(process, true);
    }
    if (config->enable_power_throttling) {
        apply_power_throttling(process, true);
    }

    bool success = EmptyWorkingSet(process) != FALSE;
    CloseHandle(process);
    return success;
}

bool snapshot_memory_totals(const BrowserGroup *groups, size_t group_count, MemoryTotals *totals) {
    PROCESS_MEMORY_COUNTERS_EX counters;

    ZeroMemory(totals, sizeof(*totals));
    for (size_t i = 0; i < group_count; ++i) {
        for (size_t j = 0; j < groups[i].pid_count; ++j) {
            HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, groups[i].pids[j]);
            if (process == NULL) {
                continue;
            }

            ZeroMemory(&counters, sizeof(counters));
            if (GetProcessMemoryInfo(process, (PROCESS_MEMORY_COUNTERS *)&counters, sizeof(counters))) {
                totals->working_set_bytes += counters.WorkingSetSize;
                totals->private_bytes += counters.PrivateUsage;
                totals->process_count += 1;
            }

            CloseHandle(process);
        }
    }

    return true;
}
