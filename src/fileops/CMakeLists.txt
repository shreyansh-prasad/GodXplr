add_library(godxplr-fileops STATIC
    src/operation_journal.cpp
    src/conflict_resolver.cpp
    src/streaming_file_executor.cpp
    src/fileops_coordinator.cpp
)

target_include_directories(godxplr-fileops
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(godxplr-fileops
    PUBLIC
        godxplr-core
        godxplr-platform-win32
)

target_compile_features(godxplr-fileops PUBLIC cxx_std_20)

