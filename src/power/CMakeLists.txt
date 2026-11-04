add_library(godxplr-power STATIC
    src/batch_renamer.cpp
    src/duplicate_finder.cpp
    src/folder_sync.cpp
)

target_include_directories(godxplr-power
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(godxplr-power
    PUBLIC
        godxplr-core
        godxplr-fileops
        godxplr-platform-win32
)

target_compile_features(godxplr-power PUBLIC cxx_std_20)

