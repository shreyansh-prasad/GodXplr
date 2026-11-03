add_library(godxplr-providers STATIC
    src/virtual_provider.cpp
    src/zip_archive_provider.cpp
    src/cloud_storage_provider.cpp
    src/network_share_provider.cpp
)

target_include_directories(godxplr-providers
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(godxplr-providers
    PUBLIC
        godxplr-core
        godxplr-platform-win32
        mpr
)

target_compile_features(godxplr-providers PUBLIC cxx_std_20)
