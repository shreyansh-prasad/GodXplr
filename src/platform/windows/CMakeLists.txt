add_library(godxplr-platform-win32 STATIC
    include/godxplr/platform/win32_common.h
    include/godxplr/platform/usn_enumerator.h
    include/godxplr/platform/usn_tail.h
    include/godxplr/platform/shm_channel.h
    include/godxplr/platform/win32_directory_provider.h
    include/godxplr/platform/com_apartment.h
    include/godxplr/platform/shell_thumbnail_provider.h
    src/win32_common.cpp
    src/usn_enumerator.cpp
    src/usn_tail.cpp
    src/shm_channel.cpp
    src/win32_directory_provider.cpp
    src/com_apartment.cpp
    src/shell_thumbnail_provider.cpp
)

target_include_directories(godxplr-platform-win32
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(godxplr-platform-win32
    PUBLIC
        godxplr-core
        kernel32
        user32
        gdi32
        advapi32
        shell32
        ole32
)

target_compile_features(godxplr-platform-win32 PUBLIC cxx_std_20)

