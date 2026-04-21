#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <stdbool.h>
#include <windows.h>

typedef enum SuspendPolicy {
    SUSPEND_POLICY_MINIMIZED_ONLY = 0,
    SUSPEND_POLICY_ALL_BACKGROUND = 1
} SuspendPolicy;

typedef struct AppConfig {
    DWORD interval_ms;
    DWORD trim_interval_ms;
    DWORD background_grace_ms;
    DWORD manual_resume_grace_ms;
    SuspendPolicy suspend_policy;
    bool trim_working_set;
    bool lower_memory_priority;
    bool enable_power_throttling;
    bool verbose;
} AppConfig;

void init_app_config(AppConfig *config);
bool parse_app_config(AppConfig *config, int argc, char **argv);
void print_usage(const char *program_name);

#endif
