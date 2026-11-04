add_library(godxplr-distribution STATIC
    include/godxplr/distribution/portable_mode.h
    include/godxplr/distribution/registry_manager.h
    include/godxplr/distribution/service_controller.h
    include/godxplr/distribution/update_verifier.h
    src/portable_mode.cpp
    src/registry_manager.cpp
    src/service_controller.cpp
    src/update_verifier.cpp
)

target_include_directories(godxplr-distribution
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(godxplr-distribution
    PUBLIC
        godxplr-core
        godxplr-platform-win32
        advapi32
        shell32
)

target_compile_features(godxplr-distribution PUBLIC cxx_std_20)

