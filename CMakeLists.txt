cmake_minimum_required(VERSION 3.20)

project(browser_guard C)

include(CTest)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(browser_guard WIN32
    src/main.c
    src/app_config.c
    src/browser_guard.c
    src/control_protocol.c
    src/process_identity.c
    src/process_control.c
    src/recovery_broker.c
    src/recovery_journal.c
    src/suspend_policy.c
)

add_executable(browser_guard_control WIN32
    src/control_main.c
    src/control_protocol.c
    src/process_identity.c
)

add_executable(browser_guard_recovery WIN32
    src/recovery_main.c
    src/process_identity.c
    src/recovery_broker.c
    src/recovery_journal.c
)

target_include_directories(browser_guard PRIVATE include)
target_include_directories(browser_guard_control PRIVATE include)
target_include_directories(browser_guard_recovery PRIVATE include)

target_compile_definitions(browser_guard PRIVATE UNICODE _UNICODE)
target_compile_definitions(browser_guard_control PRIVATE UNICODE _UNICODE)
target_compile_definitions(browser_guard_recovery PRIVATE UNICODE _UNICODE)

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

target_link_libraries(browser_guard_recovery PRIVATE
    Advapi32
    Shell32
)

if(BUILD_TESTING)
    add_executable(browser_guard_policy_tests
        tests/test_suspend_policy.c
        src/suspend_policy.c
    )

    target_include_directories(browser_guard_policy_tests PRIVATE include)
    add_test(NAME suspend_policy COMMAND browser_guard_policy_tests)

    add_executable(browser_guard_control_protocol_tests
        tests/test_control_protocol.c
        src/control_protocol.c
    )

    target_include_directories(browser_guard_control_protocol_tests PRIVATE include)
    target_compile_definitions(browser_guard_control_protocol_tests PRIVATE UNICODE _UNICODE)
    target_link_libraries(browser_guard_control_protocol_tests PRIVATE Advapi32)
    add_test(NAME control_protocol COMMAND browser_guard_control_protocol_tests)

    add_executable(browser_guard_process_identity_tests
        tests/test_process_identity.c
        src/process_identity.c
    )

    target_include_directories(browser_guard_process_identity_tests PRIVATE include)
    target_compile_definitions(browser_guard_process_identity_tests PRIVATE UNICODE _UNICODE)
    target_link_libraries(browser_guard_process_identity_tests PRIVATE Advapi32)
    add_test(NAME process_identity COMMAND browser_guard_process_identity_tests)

    add_test(
        NAME graceful_shutdown
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass
            -File ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_graceful_shutdown.ps1
            -GuardPath $<TARGET_FILE:browser_guard>
            -ControllerPath $<TARGET_FILE:browser_guard_control>
    )
    set_tests_properties(graceful_shutdown PROPERTIES TIMEOUT 15)

    add_executable(browser_guard_recovery_journal_tests
        tests/test_recovery_journal.c
        src/process_identity.c
        src/recovery_journal.c
    )

    target_include_directories(browser_guard_recovery_journal_tests PRIVATE include)
    target_compile_definitions(browser_guard_recovery_journal_tests PRIVATE UNICODE _UNICODE)
    target_link_libraries(browser_guard_recovery_journal_tests PRIVATE Advapi32)
    add_test(NAME recovery_journal COMMAND browser_guard_recovery_journal_tests)

    add_executable(browser_guard_recovery_fixture WIN32
        tests/recovery_fixture.c
    )
    target_compile_definitions(browser_guard_recovery_fixture PRIVATE UNICODE _UNICODE)
    target_link_libraries(browser_guard_recovery_fixture PRIVATE Shell32)

    add_executable(browser_guard_recovery_owner_fixture WIN32
        tests/recovery_owner_fixture.c
        src/process_identity.c
        src/recovery_broker.c
        src/recovery_journal.c
    )
    target_include_directories(browser_guard_recovery_owner_fixture PRIVATE include)
    target_compile_definitions(browser_guard_recovery_owner_fixture PRIVATE UNICODE _UNICODE)
    target_link_libraries(browser_guard_recovery_owner_fixture PRIVATE Advapi32 Shell32)

    add_test(
        NAME crash_recovery
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass
            -File ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_crash_recovery.ps1
            -OwnerPath $<TARGET_FILE:browser_guard_recovery_owner_fixture>
            -BrokerPath $<TARGET_FILE:browser_guard_recovery>
            -FixturePath $<TARGET_FILE:browser_guard_recovery_fixture>
    )
    set_tests_properties(crash_recovery PROPERTIES TIMEOUT 15)
endif()
