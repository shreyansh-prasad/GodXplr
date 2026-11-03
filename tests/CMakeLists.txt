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

add_executable(test-fuzzy-ranking
    unit/core/fuzzy_ranking_tests.cpp
)
target_link_libraries(test-fuzzy-ranking PRIVATE godxplr-core)
add_test(NAME Core.FuzzyRanking COMMAND test-fuzzy-ranking)

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

add_executable(test-navigation-state
    unit/ui/navigation_state_tests.cpp
)
target_link_libraries(test-navigation-state PRIVATE godxplr-ui-state godxplr-core)
add_test(NAME UI.NavigationState COMMAND test-navigation-state)

add_executable(test-virtualized-view
    unit/ui/virtualized_view_tests.cpp
)
target_link_libraries(test-virtualized-view PRIVATE godxplr-ui-state godxplr-core)
add_test(NAME UI.VirtualizedView COMMAND test-virtualized-view)

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

    add_executable(test-mft-raw-enumerator
        unit/platform/mft_raw_enumerator_tests.cpp
    )
    target_link_libraries(test-mft-raw-enumerator PRIVATE godxplr-platform-win32 godxplr-core)
    add_test(NAME Platform.MftRawEnumerator COMMAND test-mft-raw-enumerator)

    add_executable(test-async-pipeline
        unit/ui/async_pipeline_tests.cpp
    )
    target_link_libraries(test-async-pipeline PRIVATE godxplr-ui-state godxplr-platform-win32 godxplr-core)
    add_test(NAME UI.AsyncPipeline COMMAND test-async-pipeline)

    add_executable(test-navigation-interactions
        unit/ui/navigation_interactions_tests.cpp
    )
    target_link_libraries(test-navigation-interactions PRIVATE godxplr-ui-state godxplr-platform-win32 godxplr-core)
    add_test(NAME UI.NavigationInteractions COMMAND test-navigation-interactions)

    add_executable(test-icon-thumbnail
        unit/ui/icon_thumbnail_tests.cpp
    )
    target_link_libraries(test-icon-thumbnail PRIVATE godxplr-ui-state godxplr-platform-win32 godxplr-core)
    add_test(NAME UI.IconThumbnail COMMAND test-icon-thumbnail)

    add_executable(test-search-ipc-crawler
        unit/platform/search_ipc_crawler_tests.cpp
    )
    target_link_libraries(test-search-ipc-crawler PRIVATE godxplr-platform-win32 godxplr-core)
    add_test(NAME Platform.SearchIpcCrawler COMMAND test-search-ipc-crawler)

    add_executable(test-content-index
        unit/content/content_index_tests.cpp
    )
    target_link_libraries(test-content-index PRIVATE godxplr-content godxplr-platform-win32 godxplr-core)
    add_test(NAME Content.Index COMMAND test-content-index)

    add_executable(test-fileops
        unit/fileops/fileops_tests.cpp
    )
    target_link_libraries(test-fileops PRIVATE godxplr-fileops godxplr-platform-win32 godxplr-core)
    add_test(NAME FileOps.Engine COMMAND test-fileops)

    add_executable(test-shell-integration
        unit/platform/shell_integration_tests.cpp
    )
    target_link_libraries(test-shell-integration PRIVATE godxplr-platform-win32 godxplr-core)
    add_test(NAME Platform.ShellIntegration COMMAND test-shell-integration)

    add_executable(test-power-features
        unit/power/power_features_tests.cpp
    )
    target_link_libraries(test-power-features PRIVATE godxplr-power godxplr-fileops godxplr-platform-win32 godxplr-core)
    add_test(NAME Power.Features COMMAND test-power-features)

    add_executable(test-archive-provider
        unit/providers/archive_provider_tests.cpp
    )
    target_link_libraries(test-archive-provider PRIVATE godxplr-providers godxplr-fileops godxplr-platform-win32 godxplr-core)
    add_test(NAME Providers.Archive COMMAND test-archive-provider)
endif()

# Benchmark executable
add_executable(bench-name-index
    benchmarks/name_index_bench.cpp
)
target_link_libraries(bench-name-index PRIVATE godxplr-core)
