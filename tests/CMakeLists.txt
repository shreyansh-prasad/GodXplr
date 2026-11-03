add_executable(test-entry-key
    unit/core/entry_key_tests.cpp
)
target_link_libraries(test-entry-key PRIVATE godxplr-core)
add_test(NAME Core.EntryKey COMMAND test-entry-key)

if(WIN32)
    add_executable(test-win32-common
        unit/platform/win32_common_tests.cpp
    )
    target_link_libraries(test-win32-common PRIVATE godxplr-platform-win32)
    add_test(NAME Platform.Win32Common COMMAND test-win32-common)
endif()
