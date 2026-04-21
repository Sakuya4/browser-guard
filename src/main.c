#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "browser_guard.h"

static void print_usage(const char *program_name) {
    fprintf(stderr, "Usage: %s [--interval-ms N] [--trim-working-set] [--verbose]\n", program_name);
}

int main(int argc, char **argv) {
    AppConfig config = {
        .interval_ms = 1000,
        .trim_working_set = false,
        .verbose = false,
    };

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--trim-working-set") == 0) {
            config.trim_working_set = true;
            continue;
        }

        if (strcmp(argv[i], "--verbose") == 0) {
            config.verbose = true;
            continue;
        }

        if (strcmp(argv[i], "--interval-ms") == 0) {
            if (i + 1 >= argc) {
                print_usage(argv[0]);
                return 1;
            }

            char *end = NULL;
            unsigned long value = strtoul(argv[++i], &end, 10);
            if (end == argv[i] || *end != '\0' || value < 100 || value > 60000) {
                fprintf(stderr, "Invalid interval: %s\n", argv[i]);
                return 1;
            }

            config.interval_ms = (DWORD)value;
            continue;
        }

        print_usage(argv[0]);
        return 1;
    }

    return run_browser_guard(&config);
}
