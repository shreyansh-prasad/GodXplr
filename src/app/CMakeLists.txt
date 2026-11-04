add_executable(godxplr WIN32
    main.cpp
    main_window.h
    main_window.cpp
)

target_include_directories(godxplr
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(godxplr
    PRIVATE
        godxplr-ui-state
        godxplr-fileops
        godxplr-power
        godxplr-providers
        godxplr-distribution
        godxplr-platform-win32
        godxplr-core
        comctl32
        uxtheme
        dwmapi
        shell32
        ole32
        oleaut32
        advapi32
        user32
        gdi32
)

target_compile_features(godxplr PRIVATE cxx_std_20)

