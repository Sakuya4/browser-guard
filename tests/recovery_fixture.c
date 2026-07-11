#include <windows.h>
#include <shellapi.h>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous_instance, PWSTR command_line, int show_command) {
    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    HANDLE marker;

    (void)instance;
    (void)previous_instance;
    (void)command_line;
    (void)show_command;

    if (argv == NULL || argc != 2) {
        if (argv != NULL) {
            LocalFree(argv);
        }
        return 1;
    }

    marker = CreateFileW(argv[1], GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    LocalFree(argv);
    if (marker == INVALID_HANDLE_VALUE) {
        return 1;
    }

    CloseHandle(marker);
    return 0;
}
