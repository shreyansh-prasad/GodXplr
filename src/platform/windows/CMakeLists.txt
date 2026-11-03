add_library(godxplr-platform-win32 STATIC
    include/godxplr/platform/win32_common.h
    src/win32_common.cpp
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
        advapi32
        shell32
        ole32
)

target_compile_features(godxplr-platform-win32 PUBLIC cxx_std_20)

