add_library(godxplr-core STATIC
    include/godxplr/core/version.h
    include/godxplr/core/entry_key.h
    src/entry_key.cpp
)

target_include_directories(godxplr-core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(godxplr-core PUBLIC cxx_std_20)
