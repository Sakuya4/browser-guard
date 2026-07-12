#ifndef CONTROL_PROTOCOL_H
#define CONTROL_PROTOCOL_H

#include <windows.h>

HANDLE bg_create_shutdown_event(void);
HANDLE bg_open_shutdown_event(void);

#endif
