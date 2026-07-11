#include <stdbool.h>
#include <stdio.h>
#include <windows.h>

#include "control_protocol.h"

int main(void) {
    HANDLE owner = bg_create_shutdown_event();
    HANDLE client = NULL;
    DWORD wait_result;

    if (owner == NULL) {
        fprintf(stderr, "FAIL: guard could not create its shutdown event\n");
        return 1;
    }

    client = bg_open_shutdown_event();
    if (client == NULL) {
        fprintf(stderr, "FAIL: controller could not open the guard shutdown event\n");
        CloseHandle(owner);
        return 1;
    }

    if (!SetEvent(client)) {
        fprintf(stderr, "FAIL: controller could not signal the shutdown event\n");
        CloseHandle(client);
        CloseHandle(owner);
        return 1;
    }

    wait_result = WaitForSingleObject(owner, 1000);
    CloseHandle(client);
    CloseHandle(owner);

    if (wait_result != WAIT_OBJECT_0) {
        fprintf(stderr, "FAIL: guard did not observe the shutdown request\n");
        return 1;
    }

    return 0;
}
