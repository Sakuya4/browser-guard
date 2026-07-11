#ifndef SUSPEND_POLICY_H
#define SUSPEND_POLICY_H

#include <stdbool.h>
#include <stddef.h>

#include "app_config.h"

typedef enum SuspendReason {
    SUSPEND_REASON_ELIGIBLE = 0,
    SUSPEND_REASON_FOREGROUND,
    SUSPEND_REASON_AUDIO,
    SUSPEND_REASON_DISABLED,
    SUSPEND_REASON_MANUAL_RESUME_GRACE,
    SUSPEND_REASON_BACKGROUND_GRACE,
    SUSPEND_REASON_VISIBLE_WINDOW,
    SUSPEND_REASON_NO_VISIBLE_WINDOWS
} SuspendReason;

typedef struct SuspendPolicyInput {
    SuspendPolicy policy;
    size_t minimized_window_count;
    size_t visible_restored_window_count;
    bool has_foreground_window;
    bool has_audio;
    bool suspend_disabled;
    bool manual_resume_grace_elapsed;
    bool background_grace_elapsed;
} SuspendPolicyInput;

typedef struct SuspendDecision {
    bool should_suspend;
    SuspendReason reason;
} SuspendDecision;

bool browser_windows_are_minimized_only(
    size_t visible_window_count,
    size_t minimized_window_count,
    size_t visible_restored_window_count
);
SuspendDecision evaluate_suspend_policy(const SuspendPolicyInput *input);
const char *suspend_reason_name(SuspendReason reason);

#endif
