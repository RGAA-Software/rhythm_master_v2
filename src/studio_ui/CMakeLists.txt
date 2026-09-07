add_library(studio_ui STATIC studio.cpp node_palette.cpp semantic_palette.cpp graph_canvas.cpp node_visual.cpp curve_editor.cpp expression_editor.cpp binding_editor.cpp component_panel.cpp component_workbench.cpp component_interface.cpp property_inspector.cpp asset_panel.cpp canvas_settings.cpp input_preview.cpp timeline_panel.cpp)
target_include_directories(studio_ui PUBLIC include)
target_sources(studio_ui PRIVATE template_browser.cpp performance_panel.cpp component_library_panel.cpp preview_routing.cpp)
target_link_libraries(studio_ui PUBLIC platform_sdl PRIVATE graph_runtime editor_application project_io
    cyber_ui asset_store prepared_assets content_catalog semantic_catalog audio_ui player_core spike_imgui spike_node_editor nlohmann_json::nlohmann_json)
rhythm_project_target(studio_ui)
if(RHYTHM_BUILD_MEDIA)
    target_sources(studio_ui PRIVATE export_panel.cpp soundtrack_panel.cpp)
    target_link_libraries(studio_ui PRIVATE export_jobs music_authoring)
endif()
if(BUILD_TESTING)
    if(RHYTHM_BUILD_MEDIA)
        add_executable(soundtrack_interaction_tests tests/soundtrack_interaction_tests.cpp)
        target_include_directories(soundtrack_interaction_tests PRIVATE .)
        target_link_libraries(soundtrack_interaction_tests PRIVATE studio_ui music_authoring project_io audio_ui nlohmann_json::nlohmann_json)
        rhythm_project_target(soundtrack_interaction_tests)
        foreach(locale en-US zh-CN)
            add_test(NAME soundtrack_ui_${locale} COMMAND soundtrack_interaction_tests
                "${PROJECT_BINARY_DIR}/audio-decoder-tests/tone.flac" "${PROJECT_BINARY_DIR}/soundtrack-ui"
                "${PROJECT_SOURCE_DIR}/locales/${locale}/studio.json")
            set_tests_properties(soundtrack_ui_${locale} PROPERTIES FIXTURES_REQUIRED local_audio_media
                ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${RHYTHM_IO_SDK}/bin;PATH=path_list_prepend:${RHYTHM_MEDIA_SDK}/bin")
            add_test(NAME large_soundtrack_ui_${locale} COMMAND soundtrack_interaction_tests
                "${PROJECT_BINARY_DIR}/large-music-fixtures/resonance_demo.wav" "${PROJECT_BINARY_DIR}/large-soundtrack-ui"
                "${PROJECT_SOURCE_DIR}/locales/${locale}/studio.json")
            set_tests_properties(large_soundtrack_ui_${locale} PROPERTIES FIXTURES_REQUIRED large_music_pcm TIMEOUT 60
                ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${RHYTHM_IO_SDK}/bin;PATH=path_list_prepend:${RHYTHM_MEDIA_SDK}/bin")
        endforeach()
    endif()
    add_executable(component_library_interaction_tests tests/component_library_interaction_tests.cpp)
    target_include_directories(component_library_interaction_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(component_library_interaction_tests PRIVATE studio_ui graph_runtime spike_imgui semantic_catalog nlohmann_json::nlohmann_json)
    rhythm_project_target(component_library_interaction_tests)
    add_test(NAME component_library_interactions COMMAND component_library_interaction_tests
        "${PROJECT_BINARY_DIR}/component-library-ui" "${PROJECT_SOURCE_DIR}/locales/en-US/studio.json")
    set_tests_properties(component_library_interactions PROPERTIES TIMEOUT 30 ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${RHYTHM_SPIKE_SDK}/bin;PATH=path_list_prepend:${RHYTHM_SPIKE_SDK}/debug/bin;PATH=path_list_prepend:${RHYTHM_IO_SDK}/bin;PATH=path_list_prepend:${RHYTHM_IO_SDK}/debug/bin")
    add_executable(component_interaction_tests tests/component_interaction_tests.cpp)
    target_include_directories(component_interaction_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(component_interaction_tests PRIVATE studio_ui graph_runtime spike_imgui editor_application content_catalog)
    rhythm_project_target(component_interaction_tests)
    add_test(NAME component_interactions COMMAND component_interaction_tests)
    set_tests_properties(component_interactions PROPERTIES TIMEOUT 30 ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${RHYTHM_SPIKE_SDK}/bin;PATH=path_list_prepend:${RHYTHM_SPIKE_SDK}/debug/bin")
    add_executable(timeline_interaction_tests tests/timeline_interaction_tests.cpp timeline_panel.cpp curve_editor.cpp)
    target_include_directories(timeline_interaction_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(timeline_interaction_tests PRIVATE graph_runtime editor_application spike_imgui)
    rhythm_project_target(timeline_interaction_tests)
    add_test(NAME timeline_interactions COMMAND timeline_interaction_tests)
    set_tests_properties(timeline_interactions PROPERTIES TIMEOUT 30 ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${RHYTHM_SPIKE_SDK}/bin;PATH=path_list_prepend:${RHYTHM_SPIKE_SDK}/debug/bin")
    add_executable(curve_interaction_tests tests/curve_interaction_tests.cpp curve_editor.cpp)
    target_include_directories(curve_interaction_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(curve_interaction_tests PRIVATE parameters spike_imgui)
    rhythm_project_target(curve_interaction_tests)
    add_test(NAME curve_interactions COMMAND curve_interaction_tests)
    set_tests_properties(curve_interactions PROPERTIES TIMEOUT 30 ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${RHYTHM_SPIKE_SDK}/bin;PATH=path_list_prepend:${RHYTHM_SPIKE_SDK}/debug/bin")
    add_executable(canvas_interaction_tests tests/canvas_interaction_tests.cpp)
    target_include_directories(canvas_interaction_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(canvas_interaction_tests PRIVATE studio_ui graph_runtime spike_imgui editor_application)
    rhythm_project_target(canvas_interaction_tests)
    add_test(NAME canvas_interactions COMMAND canvas_interaction_tests)
    set_tests_properties(canvas_interactions PROPERTIES TIMEOUT 30
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${RHYTHM_SPIKE_SDK}/bin;PATH=path_list_prepend:${RHYTHM_SPIKE_SDK}/debug/bin")
endif()
