#include <stdio.h>

#include "app_config.h"

static int assert_true(bool condition, const char *scenario) {
    if (condition) {
        return 0;
    }

    fprintf(stderr, "FAIL: %s\n", scenario);
    return 1;
}

int main(void) {
    int failures = 0;
    AppConfig defaults;
    AppConfig aggressive_memory;
    AppConfig aggressive_suspend;
    AppConfig invalid;
    char *memory_args[] = {"browser_guard", "--aggressive-memory"};
    char *suspend_args[] = {"browser_guard", "--aggressive-suspend"};
    char *invalid_args[] = {"browser_guard", "--interval-ms", "99"};

    init_app_config(&defaults);
    failures += assert_true(
        defaults.suspend_policy == SUSPEND_POLICY_MINIMIZED_ONLY,
        "application default must be minimized-only"
    );
    failures += assert_true(
        !defaults.trim_working_set && !defaults.lower_memory_priority && !defaults.enable_power_throttling,
        "default resource policies must remain conservative"
    );

    init_app_config(&aggressive_memory);
    failures += assert_true(
        parse_app_config(&aggressive_memory, 2, memory_args),
        "aggressive-memory option must parse"
    );
    failures += assert_true(
        aggressive_memory.trim_working_set &&
        aggressive_memory.lower_memory_priority &&
        aggressive_memory.enable_power_throttling,
        "aggressive-memory enables only memory and power policies"
    );
    failures += assert_true(
        aggressive_memory.suspend_policy == SUSPEND_POLICY_MINIMIZED_ONLY,
        "aggressive-memory must not silently enable aggressive suspension"
    );

    init_app_config(&aggressive_suspend);
    failures += assert_true(
        parse_app_config(&aggressive_suspend, 2, suspend_args) &&
        aggressive_suspend.suspend_policy == SUSPEND_POLICY_ALL_BACKGROUND,
        "aggressive suspension requires its explicit option"
    );

    init_app_config(&invalid);
    failures += assert_true(
        !parse_app_config(&invalid, 3, invalid_args),
        "out-of-range polling interval must be rejected"
    );

    return failures == 0 ? 0 : 1;
}
