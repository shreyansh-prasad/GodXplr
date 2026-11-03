add_library(godxplr-ui-state STATIC
    state/include/godxplr/ui/state/tab_state.h
    state/include/godxplr/ui/state/navigation_coordinator.h
    state/src/navigation_coordinator.cpp
    theme/include/godxplr/ui/theme/design_tokens.h
    theme/include/godxplr/ui/theme/theme_manager.h
    theme/src/theme_manager.cpp
    shell/include/godxplr/ui/shell/breadcrumb.h
    shell/include/godxplr/ui/shell/action_registry.h
    shell/include/godxplr/ui/shell/sidebar_model.h
    shell/src/breadcrumb.cpp
    shell/src/sidebar_model.cpp
    model/include/godxplr/ui/model/geometry_types.h
    model/include/godxplr/ui/model/directory_entry.h
    model/include/godxplr/ui/model/directory_store.h
    model/include/godxplr/ui/model/selection_model.h
    model/include/godxplr/ui/model/tree_model.h
    model/src/directory_store.cpp
    model/src/selection_model.cpp
    model/src/tree_model.cpp
    view/include/godxplr/ui/view/file_layout.h
    view/include/godxplr/ui/view/details_layout.h
    view/include/godxplr/ui/view/list_layout.h
    view/include/godxplr/ui/view/grid_layout.h
    view/include/godxplr/ui/view/gallery_layout.h
    view/include/godxplr/ui/view/accessibility_semantics.h
    view/include/godxplr/ui/view/interaction_controller.h
    view/src/details_layout.cpp
    view/src/list_layout.cpp
    view/src/grid_layout.cpp
    view/src/gallery_layout.cpp
    view/src/interaction_controller.cpp
    async/include/godxplr/ui/async/ui_mailbox.h
    async/include/godxplr/ui/async/directory_sorter.h
    async/include/godxplr/ui/async/directory_pipeline_coordinator.h
    async/src/ui_mailbox.cpp
    async/src/directory_sorter.cpp
    async/src/directory_pipeline_coordinator.cpp
)

target_include_directories(godxplr-ui-state
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/state/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/theme/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/shell/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/model/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/view/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/async/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(godxplr-ui-state
    PUBLIC
        godxplr-core
        godxplr-platform-win32
)

if(WIN32)
    target_link_libraries(godxplr-ui-state
        PUBLIC
            advapi32
            user32
    )
endif()

target_compile_features(godxplr-ui-state PUBLIC cxx_std_20)
