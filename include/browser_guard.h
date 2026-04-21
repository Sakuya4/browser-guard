#ifndef BROWSER_GUARD_H
#define BROWSER_GUARD_H

#include <stdbool.h>
#include <windows.h>

typedef struct AppConfig {
    DWORD interval_ms;
    bool trim_working_set;
    bool verbose;
} AppConfig;

int run_browser_guard(const AppConfig *config);

#endif
