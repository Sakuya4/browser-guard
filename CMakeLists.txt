cmake_minimum_required(VERSION 3.20)

project(browser_guard C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(browser_guard
    src/main.c
    src/app_config.c
    src/browser_guard.c
    src/process_control.c
)

target_include_directories(browser_guard PRIVATE include)

target_compile_definitions(browser_guard PRIVATE UNICODE _UNICODE)

target_link_libraries(browser_guard PRIVATE
    Advapi32
    Ole32
    Uuid
    Psapi
)
