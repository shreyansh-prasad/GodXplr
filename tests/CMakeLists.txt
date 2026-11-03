add_executable(test-entry-key
    unit/core/entry_key_tests.cpp
)
target_link_libraries(test-entry-key PRIVATE godxplr-core)
add_test(NAME Core.EntryKey COMMAND test-entry-key)

add_executable(test-name-index
    unit/core/name_index_tests.cpp
)
target_link_libraries(test-name-index PRIVATE godxplr-core)
add_test(NAME Core.NameIndex COMMAND test-name-index)

add_executable(test-simd-search
    unit/core/simd_search_tests.cpp
)
target_link_libraries(test-simd-search PRIVATE godxplr-core)
add_test(NAME Core.SimdSearch COMMAND test-simd-search)

add_executable(test-query-parser
    unit/core/query_parser_tests.cpp
)
target_link_libraries(test-query-parser PRIVATE godxplr-core)
add_test(NAME Core.QueryParser COMMAND test-query-parser)

add_executable(test-search-engine
    unit/core/search_engine_tests.cpp
)
target_link_libraries(test-search-engine PRIVATE godxplr-core)
add_test(NAME Core.SearchEngine COMMAND test-search-engine)

add_executable(test-index-serializer
    unit/core/index_serializer_tests.cpp
)
target_link_libraries(test-index-serializer PRIVATE godxplr-core)
add_test(NAME Core.IndexSerializer COMMAND test-index-serializer)

add_executable(test-dataset-generator
    unit/bench/dataset_generator_tests.cpp
)
target_link_libraries(test-dataset-generator PRIVATE godxplr-bench godxplr-core)
add_test(NAME Bench.DatasetGenerator COMMAND test-dataset-generator)

if(WIN32)
    add_executable(test-win32-common
        unit/platform/win32_common_tests.cpp
    )
    target_link_libraries(test-win32-common PRIVATE godxplr-platform-win32)
    add_test(NAME Platform.Win32Common COMMAND test-win32-common)

    add_executable(test-shm-channel
        unit/platform/shm_channel_tests.cpp
    )
    target_link_libraries(test-shm-channel PRIVATE godxplr-platform-win32)
    add_test(NAME Platform.ShmChannel COMMAND test-shm-channel)

    add_executable(test-usn-enumerator
        unit/platform/usn_enumerator_tests.cpp
    )
    target_link_libraries(test-usn-enumerator PRIVATE godxplr-platform-win32)
    add_test(NAME Platform.UsnEnumerator COMMAND test-usn-enumerator)
endif()

# Benchmark executable
add_executable(bench-name-index
    benchmarks/name_index_bench.cpp
)
target_link_libraries(bench-name-index PRIVATE godxplr-core)
