cmake_minimum_required(VERSION 3.20)

project(browser_guard C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(browser_guard WIN32
    src/main.c
    src/app_config.c
    src/browser_guard.c
    src/process_control.c
)

add_executable(browser_guard_control WIN32
    src/control_main.c
)

target_include_directories(browser_guard PRIVATE include)
target_include_directories(browser_guard_control PRIVATE include)

target_compile_definitions(browser_guard PRIVATE UNICODE _UNICODE)
target_compile_definitions(browser_guard_control PRIVATE UNICODE _UNICODE)

target_link_libraries(browser_guard PRIVATE
    Advapi32
    Ole32
    Uuid
    Psapi
)

target_link_libraries(browser_guard_control PRIVATE
    Advapi32
    Shell32
    User32
)
