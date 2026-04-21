#define COBJMACROS
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <stdio.h>
#include <wchar.h>

#include "browser_guard.h"

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

typedef struct BrowserGroup {
    wchar_t exe_name[MAX_PATH];
    DWORD pids[256];
    size_t pid_count;
    bool has_foreground_window;
    bool has_audio;
    bool seen;
} BrowserGroup;

typedef struct TrackedProcess {
    DWORD pid;
    wchar_t exe_name[MAX_PATH];
    bool suspended;
    bool seen_this_pass;
} TrackedProcess;

static volatile BOOL g_should_stop = FALSE;
static const wchar_t *k_supported_browsers[] = {
    L"chrome.exe",
    L"msedge.exe",
    L"firefox.exe",
    L"brave.exe",
    L"opera.exe",
    L"vivaldi.exe",
};

static BOOL WINAPI handle_console_signal(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT || signal == CTRL_CLOSE_EVENT) {
        g_should_stop = TRUE;
        return TRUE;
    }

    return FALSE;
}

static bool is_supported_browser_name(const wchar_t *exe_name) {
    for (size_t i = 0; i < sizeof(k_supported_browsers) / sizeof(k_supported_browsers[0]); ++i) {
        if (_wcsicmp(exe_name, k_supported_browsers[i]) == 0) {
            return true;
        }
    }

    return false;
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
    group->seen = true;
    *group_count += 1;
    return group;
}

static void collect_browser_processes(BrowserGroup *groups, size_t *group_count, size_t capacity) {
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
        if (!is_supported_browser_name(entry.szExeFile)) {
            continue;
        }

        BrowserGroup *group = get_or_add_group(groups, group_count, capacity, entry.szExeFile);
        if (group == NULL || group->pid_count >= sizeof(group->pids) / sizeof(group->pids[0])) {
            continue;
        }

        group->pids[group->pid_count++] = entry.th32ProcessID;
        group->seen = true;
    } while (Process32NextW(snapshot, &entry));

    CloseHandle(snapshot);
}

typedef struct WindowScanContext {
    BrowserGroup *groups;
    size_t group_count;
    HWND foreground_window;
} WindowScanContext;

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

    if (hwnd == context->foreground_window && !IsIconic(hwnd)) {
        group->has_foreground_window = true;
    }

    return TRUE;
}

static void mark_foreground_groups(BrowserGroup *groups, size_t group_count) {
    WindowScanContext context = {
        .groups = groups,
        .group_count = group_count,
        .foreground_window = GetForegroundWindow(),
    };

    EnumWindows(enum_windows_callback, (LPARAM)&context);
}

static bool pid_exists_in_array(const DWORD *pids, size_t count, DWORD pid) {
    for (size_t i = 0; i < count; ++i) {
        if (pids[i] == pid) {
            return true;
        }
    }

    return false;
}

static void collect_active_audio_pids(DWORD *audio_pids, size_t *audio_pid_count, size_t capacity) {
    *audio_pid_count = 0;

    IMMDeviceEnumerator *device_enumerator = NULL;
    IMMDevice *default_device = NULL;
    IAudioSessionManager2 *session_manager = NULL;
    IAudioSessionEnumerator *session_enumerator = NULL;
    HRESULT hr = CoCreateInstance(
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

static void mark_audio_groups(BrowserGroup *groups, size_t group_count, const DWORD *audio_pids, size_t audio_pid_count) {
    for (size_t i = 0; i < group_count; ++i) {
        for (size_t j = 0; j < groups[i].pid_count; ++j) {
            if (pid_exists_in_array(audio_pids, audio_pid_count, groups[i].pids[j])) {
                groups[i].has_audio = true;
                break;
            }
        }
    }
}

static TrackedProcess *find_tracked_process(TrackedProcess *tracked, size_t tracked_count, DWORD pid) {
    for (size_t i = 0; i < tracked_count; ++i) {
        if (tracked[i].pid == pid) {
            return &tracked[i];
        }
    }

    return NULL;
}

static bool suspend_or_resume_process(DWORD pid, bool suspend, bool trim_working_set) {
    bool success = false;
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (ntdll == NULL) {
        return false;
    }

    NtSuspendProcessFn nt_suspend = (NtSuspendProcessFn)GetProcAddress(ntdll, "NtSuspendProcess");
    NtResumeProcessFn nt_resume = (NtResumeProcessFn)GetProcAddress(ntdll, "NtResumeProcess");
    if (nt_suspend == NULL || nt_resume == NULL) {
        return false;
    }

    DWORD access_mask = PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_SUSPEND_RESUME;
    if (trim_working_set) {
        access_mask |= PROCESS_SET_QUOTA;
    }

    HANDLE process = OpenProcess(access_mask, FALSE, pid);
    if (process == NULL) {
        return false;
    }

    NTSTATUS status = suspend ? nt_suspend(process) : nt_resume(process);
    success = (status >= 0);

    if (success && suspend && trim_working_set) {
        EmptyWorkingSet(process);
    }

    CloseHandle(process);
    return success;
}

static void cleanup_tracked_processes(TrackedProcess *tracked, size_t *tracked_count, const AppConfig *config) {
    size_t write_index = 0;
    for (size_t i = 0; i < *tracked_count; ++i) {
        if (!tracked[i].seen_this_pass) {
            if (tracked[i].suspended) {
                suspend_or_resume_process(tracked[i].pid, false, false);
            }
            continue;
        }

        tracked[i].seen_this_pass = false;
        tracked[write_index++] = tracked[i];
    }

    *tracked_count = write_index;
}

static void ensure_group_state(
    const BrowserGroup *group,
    TrackedProcess *tracked,
    size_t *tracked_count,
    size_t tracked_capacity,
    const AppConfig *config
) {
    bool should_suspend = !group->has_foreground_window && !group->has_audio;

    for (size_t i = 0; i < group->pid_count; ++i) {
        DWORD pid = group->pids[i];
        TrackedProcess *entry = find_tracked_process(tracked, *tracked_count, pid);

        if (entry == NULL) {
            if (*tracked_count >= tracked_capacity) {
                continue;
            }

            entry = &tracked[*tracked_count];
            ZeroMemory(entry, sizeof(*entry));
            entry->pid = pid;
            wcsncpy(entry->exe_name, group->exe_name, MAX_PATH - 1);
            *tracked_count += 1;
        }

        entry->seen_this_pass = true;

        if (entry->suspended == should_suspend) {
            continue;
        }

        if (!suspend_or_resume_process(pid, should_suspend, config->trim_working_set)) {
            if (config->verbose) {
                fwprintf(stdout, L"[warn] unable to change pid=%lu (%ls)\n", pid, group->exe_name);
            }
            continue;
        }

        entry->suspended = should_suspend;
        if (config->verbose) {
            fwprintf(
                stdout,
                should_suspend ? L"[suspend] pid=%lu (%ls)\n" : L"[resume] pid=%lu (%ls)\n",
                pid,
                group->exe_name
            );
        }
    }
}

static void resume_all_tracked(TrackedProcess *tracked, size_t tracked_count) {
    for (size_t i = 0; i < tracked_count; ++i) {
        if (tracked[i].suspended) {
            suspend_or_resume_process(tracked[i].pid, false, false);
            tracked[i].suspended = false;
        }
    }
}

int run_browser_guard(const AppConfig *config) {
    BrowserGroup groups[16];
    TrackedProcess tracked[512];
    size_t group_count = 0;
    size_t tracked_count = 0;
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        fprintf(stderr, "CoInitializeEx failed: 0x%08lx\n", (unsigned long)hr);
        return 1;
    }

    if (!SetConsoleCtrlHandler(handle_console_signal, TRUE)) {
        fprintf(stderr, "Failed to install console control handler.\n");
        CoUninitialize();
        return 1;
    }

    while (!g_should_stop) {
        DWORD audio_pids[512];
        size_t audio_pid_count = 0;

        ZeroMemory(groups, sizeof(groups));
        group_count = 0;
        collect_browser_processes(groups, &group_count, sizeof(groups) / sizeof(groups[0]));
        mark_foreground_groups(groups, group_count);
        collect_active_audio_pids(audio_pids, &audio_pid_count, sizeof(audio_pids) / sizeof(audio_pids[0]));
        mark_audio_groups(groups, group_count, audio_pids, audio_pid_count);

        for (size_t i = 0; i < group_count; ++i) {
            ensure_group_state(&groups[i], tracked, &tracked_count, sizeof(tracked) / sizeof(tracked[0]), config);
        }

        cleanup_tracked_processes(tracked, &tracked_count, config);
        Sleep(config->interval_ms);
    }

    resume_all_tracked(tracked, tracked_count);
    CoUninitialize();
    return 0;
}
