#include "suspend_policy.h"

bool browser_windows_are_minimized_only(
    size_t visible_window_count,
    size_t minimized_window_count,
    size_t visible_restored_window_count
) {
    return visible_window_count > 0 &&
           minimized_window_count == visible_window_count &&
           visible_restored_window_count == 0;
}

SuspendDecision evaluate_suspend_policy(const SuspendPolicyInput *input) {
    size_t visible_window_count;

    if (input == NULL) {
        return (SuspendDecision){false, SUSPEND_REASON_DISABLED};
    }
    if (input->has_foreground_window) {
        return (SuspendDecision){false, SUSPEND_REASON_FOREGROUND};
    }
    if (input->has_audio) {
        return (SuspendDecision){false, SUSPEND_REASON_AUDIO};
    }
    if (input->suspend_disabled) {
        return (SuspendDecision){false, SUSPEND_REASON_DISABLED};
    }
    if (!input->manual_resume_grace_elapsed) {
        return (SuspendDecision){false, SUSPEND_REASON_MANUAL_RESUME_GRACE};
    }
    if (!input->background_grace_elapsed) {
        return (SuspendDecision){false, SUSPEND_REASON_BACKGROUND_GRACE};
    }
    if (input->policy == SUSPEND_POLICY_ALL_BACKGROUND) {
        return (SuspendDecision){true, SUSPEND_REASON_ELIGIBLE};
    }

    visible_window_count = input->minimized_window_count + input->visible_restored_window_count;
    if (visible_window_count == 0) {
        return (SuspendDecision){false, SUSPEND_REASON_NO_VISIBLE_WINDOWS};
    }
    if (!browser_windows_are_minimized_only(
            visible_window_count,
            input->minimized_window_count,
            input->visible_restored_window_count)) {
        return (SuspendDecision){false, SUSPEND_REASON_VISIBLE_WINDOW};
    }

    return (SuspendDecision){true, SUSPEND_REASON_ELIGIBLE};
}

const char *suspend_reason_name(SuspendReason reason) {
    switch (reason) {
        case SUSPEND_REASON_ELIGIBLE:
            return "eligible";
        case SUSPEND_REASON_FOREGROUND:
            return "foreground";
        case SUSPEND_REASON_AUDIO:
            return "audio";
        case SUSPEND_REASON_DISABLED:
            return "disabled";
        case SUSPEND_REASON_MANUAL_RESUME_GRACE:
            return "manual-resume-grace";
        case SUSPEND_REASON_BACKGROUND_GRACE:
            return "background-grace";
        case SUSPEND_REASON_VISIBLE_WINDOW:
            return "visible-window";
        case SUSPEND_REASON_NO_VISIBLE_WINDOWS:
            return "no-visible-windows";
        default:
            return "unknown";
    }
}
