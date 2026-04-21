#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app_config.h"

static bool parse_dword_arg(const char *value, DWORD min_value, DWORD max_value, DWORD *out_value) {
    char *end = NULL;
    unsigned long parsed = strtoul(value, &end, 10);
    if (end == value || *end != '\0' || parsed < min_value || parsed > max_value) {
        return false;
    }

    *out_value = (DWORD)parsed;
    return true;
}

void init_app_config(AppConfig *config) {
    config->interval_ms = 1000;
    config->trim_interval_ms = 5000;
    config->trim_working_set = false;
    config->lower_memory_priority = false;
    config->enable_power_throttling = false;
    config->verbose = false;
}

void print_usage(const char *program_name) {
    fprintf(stderr, "Usage: %s [options]\n", program_name);
    fprintf(stderr, "  --interval-ms N           Polling interval in milliseconds (100-60000)\n");
    fprintf(stderr, "  --trim-working-set        Trim browser working sets after suspension\n");
    fprintf(stderr, "  --trim-interval-ms N      Re-trim suspended browsers every N ms (1000-60000)\n");
    fprintf(stderr, "  --lower-memory-priority   Lower browser memory priority while suspended\n");
    fprintf(stderr, "  --eco-qos                 Enable Windows power throttling while suspended\n");
    fprintf(stderr, "  --aggressive-memory       Enable trim, low memory priority, and EcoQoS together\n");
    fprintf(stderr, "  --verbose                 Print lifecycle and memory decisions\n");
}

bool parse_app_config(AppConfig *config, int argc, char **argv) {
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--trim-working-set") == 0) {
            config->trim_working_set = true;
            continue;
        }

        if (strcmp(argv[i], "--lower-memory-priority") == 0) {
            config->lower_memory_priority = true;
            continue;
        }

        if (strcmp(argv[i], "--eco-qos") == 0) {
            config->enable_power_throttling = true;
            continue;
        }

        if (strcmp(argv[i], "--aggressive-memory") == 0) {
            config->trim_working_set = true;
            config->lower_memory_priority = true;
            config->enable_power_throttling = true;
            continue;
        }

        if (strcmp(argv[i], "--verbose") == 0) {
            config->verbose = true;
            continue;
        }

        if (strcmp(argv[i], "--interval-ms") == 0) {
            if (i + 1 >= argc || !parse_dword_arg(argv[++i], 100, 60000, &config->interval_ms)) {
                return false;
            }
            continue;
        }

        if (strcmp(argv[i], "--trim-interval-ms") == 0) {
            if (i + 1 >= argc || !parse_dword_arg(argv[++i], 1000, 60000, &config->trim_interval_ms)) {
                return false;
            }
            continue;
        }

        return false;
    }

    if (!config->trim_working_set && config->trim_interval_ms != 5000) {
        fprintf(stderr, "--trim-interval-ms requires --trim-working-set.\n");
        return false;
    }

    return true;
}
