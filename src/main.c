#include "app_config.h"
#include "browser_guard.h"

#ifdef _WIN32
#include <windows.h>

static int run_entrypoint(int argc, char **argv) {
    AppConfig config;

    init_app_config(&config);
    if (!parse_app_config(&config, argc, argv)) {
        print_usage(argv[0]);
        return 1;
    }

    return run_browser_guard(&config);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous_instance, LPSTR command_line, int show_command) {
    (void)instance;
    (void)previous_instance;
    (void)command_line;
    (void)show_command;
    return run_entrypoint(__argc, __argv);
}
#else
int main(int argc, char **argv) {
    AppConfig config;

    init_app_config(&config);
    if (!parse_app_config(&config, argc, argv)) {
        print_usage(argv[0]);
        return 1;
    }

    return run_browser_guard(&config);
}
#endif
