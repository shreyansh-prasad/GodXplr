add_library(godxplr-core STATIC
    include/godxplr/core/version.h
    include/godxplr/core/entry_key.h
    include/godxplr/core/directory_entry.h
    include/godxplr/core/provider_contract.h
    include/godxplr/core/search_types.h
    include/godxplr/core/name_index.h
    include/godxplr/core/simd_search.h
    include/godxplr/core/query_ast.h
    include/godxplr/core/query_parser.h
    include/godxplr/core/search_engine.h
    include/godxplr/core/index_serializer.h
    include/godxplr/core/pixel_buffer.h
    include/godxplr/core/fuzzy_matcher.h
    include/godxplr/core/ranking.h
    include/godxplr/core/usage_db.h
    src/entry_key.cpp
    src/name_index.cpp
    src/simd_search.cpp
    src/query_parser.cpp
    src/search_engine.cpp
    src/index_serializer.cpp
    src/fuzzy_matcher.cpp
    src/ranking.cpp
    src/usage_db.cpp
)

target_include_directories(godxplr-core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(godxplr-core PUBLIC cxx_std_20)
