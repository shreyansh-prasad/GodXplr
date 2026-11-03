add_library(godxplr-content STATIC
    src/encoding_detector.cpp
    src/text_extractor.cpp
    src/trigram_index.cpp
    src/content_search_engine.cpp
    src/snippet_generator.cpp
    src/content_queue.cpp
    src/ooxml_extractor.cpp
    src/extractor_sandbox.cpp
)

target_include_directories(godxplr-content
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(godxplr-content
    PUBLIC
        godxplr-core
        godxplr-platform-win32
)

target_compile_features(godxplr-content PUBLIC cxx_std_20)
