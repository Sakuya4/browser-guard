#ifndef SUSPEND_POLICY_H
#define SUSPEND_POLICY_H

#include <stdbool.h>
#include <stddef.h>

bool browser_windows_are_minimized_only(
    size_t visible_window_count,
    size_t minimized_window_count,
    size_t visible_restored_window_count
);

#endif
