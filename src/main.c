#include "app_config.h"
#include "browser_guard.h"

int main(int argc, char **argv) {
    AppConfig config;

    init_app_config(&config);
    if (!parse_app_config(&config, argc, argv)) {
        print_usage(argv[0]);
        return 1;
    }

    return run_browser_guard(&config);
}
