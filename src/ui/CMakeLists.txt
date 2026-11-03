add_library(godxplr-ui-state STATIC
    state/include/godxplr/ui/state/tab_state.h
    state/include/godxplr/ui/state/navigation_coordinator.h
    state/src/navigation_coordinator.cpp
    theme/include/godxplr/ui/theme/design_tokens.h
    theme/include/godxplr/ui/theme/theme_manager.h
    theme/src/theme_manager.cpp
    shell/include/godxplr/ui/shell/breadcrumb.h
    shell/include/godxplr/ui/shell/action_registry.h
    shell/src/breadcrumb.cpp
)

target_include_directories(godxplr-ui-state
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/state/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/theme/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/shell/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(godxplr-ui-state
    PUBLIC
        godxplr-core
)

if(WIN32)
    target_link_libraries(godxplr-ui-state
        PUBLIC
            advapi32
            user32
    )
endif()

target_compile_features(godxplr-ui-state PUBLIC cxx_std_20)
