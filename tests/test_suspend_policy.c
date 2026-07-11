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

int main(void) {
    int failures = 0;

    failures += assert_false(
        browser_windows_are_minimized_only(2, 1, 1),
        "a restored visible window must protect a browser family even when another window is minimized"
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
