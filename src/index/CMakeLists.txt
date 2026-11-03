add_executable(godxplr-indexd
    src/main_indexd.cpp
)

target_link_libraries(godxplr-indexd
    PRIVATE
        godxplr-core
        godxplr-platform-win32
)

target_compile_features(godxplr-indexd PRIVATE cxx_std_20)
