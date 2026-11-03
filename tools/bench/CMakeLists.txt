add_library(godxplr-bench STATIC
    include/godxplr/bench/dataset_generator.h
    include/godxplr/bench/metrics_recorder.h
    src/dataset_generator.cpp
    src/metrics_recorder.cpp
)

target_include_directories(godxplr-bench
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(godxplr-bench
    PUBLIC
        godxplr-core
)

if(WIN32)
    target_link_libraries(godxplr-bench
        PUBLIC
            godxplr-platform-win32
            psapi
            advapi32
    )
endif()

target_compile_features(godxplr-bench PUBLIC cxx_std_20)

# CLI Dataset Generator Tool
add_executable(bench-dataset-gen
    src/main_dataset_gen.cpp
)
target_link_libraries(bench-dataset-gen PRIVATE godxplr-bench)

# CLI Benchmark Runner Tool
add_executable(bench-runner
    src/main_bench_runner.cpp
)
target_link_libraries(bench-runner PRIVATE godxplr-bench)
