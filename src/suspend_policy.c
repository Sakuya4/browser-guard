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
