#include <stdio.h>

#include "suspend_policy.h"

static int assert_false(bool actual, const char *scenario) {
    if (!actual) {
        return 0;
    }

    fprintf(stderr, "FAIL: %s\n", scenario);
    return 1;
}

static int assert_true(bool actual, const char *scenario) {
    if (actual) {
        return 0;
    }

    fprintf(stderr, "FAIL: %s\n", scenario);
    return 1;
}

static int assert_decision(
    SuspendPolicyInput input,
    bool expected_suspend,
    SuspendReason expected_reason,
    const char *scenario
) {
    SuspendDecision decision = evaluate_suspend_policy(&input);

    if (decision.should_suspend == expected_suspend && decision.reason == expected_reason) {
        return 0;
    }

    fprintf(
        stderr,
        "FAIL: %s (suspend=%d reason=%s)\n",
        scenario,
        decision.should_suspend,
        suspend_reason_name(decision.reason)
    );
    return 1;
}

int main(void) {
    int failures = 0;

    failures += assert_false(
        browser_windows_are_minimized_only(2, 1, 1),
        "a restored visible window must protect a browser family even when another window is minimized"
    );

    SuspendPolicyInput base = {
        .policy = SUSPEND_POLICY_MINIMIZED_ONLY,
        .minimized_window_count = 1,
        .visible_restored_window_count = 0,
        .manual_resume_grace_elapsed = true,
        .background_grace_elapsed = true,
    };
    failures += assert_decision(base, true, SUSPEND_REASON_ELIGIBLE, "an idle minimized family is eligible");

    SuspendPolicyInput mixed = base;
    mixed.visible_restored_window_count = 1;
    failures += assert_decision(mixed, false, SUSPEND_REASON_VISIBLE_WINDOW, "a visible restored window explains protection");

    SuspendPolicyInput foreground = base;
    foreground.has_foreground_window = true;
    failures += assert_decision(foreground, false, SUSPEND_REASON_FOREGROUND, "foreground protection has an explicit reason");

    SuspendPolicyInput audio = base;
    audio.has_audio = true;
    failures += assert_decision(audio, false, SUSPEND_REASON_AUDIO, "active audio protection has an explicit reason");

    SuspendPolicyInput manual_grace = base;
    manual_grace.manual_resume_grace_elapsed = false;
    failures += assert_decision(
        manual_grace,
        false,
        SUSPEND_REASON_MANUAL_RESUME_GRACE,
        "manual resume grace protects the browser"
    );

    SuspendPolicyInput no_windows = base;
    no_windows.minimized_window_count = 0;
    failures += assert_decision(
        no_windows,
        false,
        SUSPEND_REASON_NO_VISIBLE_WINDOWS,
        "missing window evidence fails closed in minimized-only mode"
    );
    failures += assert_true(
        browser_windows_are_minimized_only(2, 2, 0),
        "a browser family with only minimized windows is eligible for minimized-only policy"
    );
    failures += assert_false(
        browser_windows_are_minimized_only(1, 0, 1),
        "a restored visible window is not minimized-only"
    );
    failures += assert_false(
        browser_windows_are_minimized_only(0, 0, 0),
        "a browser family with no inspected windows must fail closed"
    );

    return failures == 0 ? 0 : 1;
}
