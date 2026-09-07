add_library(render_bgfx STATIC ../rhythm_render/src/bgfx_backend.cpp ../rhythm_render/src/bgfx_readbacks.cpp)
target_include_directories(render_bgfx PRIVATE ../rhythm_render/src
    "${rhythm_deps}/bgfx/examples/common/imgui")
target_link_libraries(render_bgfx PRIVATE Rhythm::Render spike_bgfx spike_bx)
target_compile_definitions(render_bgfx PRIVATE BX_CONFIG_DEBUG=0)
rhythm_project_target(render_bgfx)
include("${PROJECT_SOURCE_DIR}/cmake/RenderShaders.cmake")
add_library(platform_sdl STATIC host.cpp ui_bridge.cpp)
target_include_directories(platform_sdl PUBLIC include PRIVATE ../rhythm_render/src)
target_link_libraries(platform_sdl PUBLIC Rhythm::Render PRIVATE render_bgfx SDL3::SDL3-static spike_imgui)
rhythm_project_target(platform_sdl)
add_subdirectory(../cyber_ui "${PROJECT_BINARY_DIR}/src/cyber_ui")
add_subdirectory(../audio_capture "${PROJECT_BINARY_DIR}/src/audio_capture")
add_subdirectory(../audio_output "${PROJECT_BINARY_DIR}/src/audio_output")
if(RHYTHM_BUILD_MEDIA)
    add_subdirectory(../audio_playback "${PROJECT_BINARY_DIR}/src/audio_playback")
endif()
add_subdirectory(../audio_ui "${PROJECT_BINARY_DIR}/src/audio_ui")
if(RHYTHM_BUILD_MEDIA)
    add_subdirectory(../export_jobs "${PROJECT_BINARY_DIR}/src/export_jobs")
endif()
add_subdirectory(../studio_ui "${PROJECT_BINARY_DIR}/src/studio_ui")
add_executable(rhythm_master main.cpp)
target_link_libraries(rhythm_master PRIVATE platform_sdl studio_ui graph_runtime)
if(RHYTHM_BUILD_MEDIA)
    target_link_libraries(rhythm_master PRIVATE export_job_host)
    target_compile_definitions(rhythm_master PRIVATE RHYTHM_HAS_EXPORT=1)
endif()
rhythm_project_target(rhythm_master)
add_custom_target(studio_resources ALL
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/sync-resources.py"
        "${PROJECT_SOURCE_DIR}/content/presets" "${PROJECT_BINARY_DIR}/content/presets"
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/sync-resources.py"
        "${PROJECT_BINARY_DIR}/content" "${CMAKE_CURRENT_BINARY_DIR}/content"
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/sync-resources.py"
        "${PROJECT_SOURCE_DIR}/locales" "${CMAKE_CURRENT_BINARY_DIR}/locales"
    DEPENDS phase_a_content semantic_content runtime_builtin_content demo_audio_content)
add_dependencies(rhythm_master studio_resources)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
# CMake supplies target metadata; Python owns dependency discovery and packaging.
set(rhythm_deploy_config "${CMAKE_CURRENT_BINARY_DIR}/deploy-config-$<CONFIG>.txt")
file(GENERATE OUTPUT "${rhythm_deploy_config}" CONTENT
"executable=$<TARGET_FILE:rhythm_master>
runtime_dlls=$<TARGET_RUNTIME_DLLS:rhythm_master>
configuration=$<CONFIG>
compiler=${CMAKE_CXX_COMPILER}
source_root=${PROJECT_SOURCE_DIR}
build_root=${PROJECT_BINARY_DIR}
sdk=${RHYTHM_SPIKE_SDK}
io_sdk=${RHYTHM_IO_SDK}
media_sdk=${RHYTHM_MEDIA_SDK}
")
set(rhythm_deploy_command "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/deploy-windows.py"
    --config "${rhythm_deploy_config}")
add_custom_command(TARGET rhythm_master POST_BUILD COMMAND ${rhythm_deploy_command} VERBATIM)
# Also deploy on resource-only and no-op default builds.
add_custom_target(studio_deploy ALL COMMAND ${rhythm_deploy_command}
    DEPENDS rhythm_master studio_resources VERBATIM)
if(BUILD_TESTING)
    add_executable(readback_gpu_tests tests/readback_gpu_tests.cpp
        ../rhythm_render/tests/readback_gpu_contracts.cpp)
    target_link_libraries(readback_gpu_tests PRIVATE platform_sdl)
    rhythm_project_target(readback_gpu_tests)
    add_test(NAME readback_gpu COMMAND readback_gpu_tests)
    set_tests_properties(readback_gpu PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE)
    add_executable(budget_gpu_tests tests/budget_gpu_tests.cpp)
    target_link_libraries(budget_gpu_tests PRIVATE studio_ui platform_sdl project_io graph_runtime spike_imgui)
    rhythm_project_target(budget_gpu_tests)
    add_dependencies(budget_gpu_tests studio_resources)
    add_test(NAME budget_gpu COMMAND budget_gpu_tests "${CMAKE_CURRENT_BINARY_DIR}"
        "${PROJECT_BINARY_DIR}/budget-gpu-projects")
    set_tests_properties(budget_gpu PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE)
    add_executable(editor_benchmark tests/editor_benchmark.cpp)
    target_link_libraries(editor_benchmark PRIVATE studio_ui platform_sdl project_io spike_imgui)
    rhythm_project_target(editor_benchmark)
    add_dependencies(editor_benchmark studio_resources)
    add_executable(component_preview_gpu_tests tests/component_preview_gpu_tests.cpp)
    target_link_libraries(component_preview_gpu_tests PRIVATE studio_ui platform_sdl project_io spike_imgui spike_bgfx)
    rhythm_project_target(component_preview_gpu_tests)
    add_dependencies(component_preview_gpu_tests studio_resources)
    add_test(NAME component_preview_gpu COMMAND component_preview_gpu_tests "${CMAKE_CURRENT_BINARY_DIR}"
        "${PROJECT_BINARY_DIR}/content/templates/resonance_live" "${PROJECT_BINARY_DIR}/component-preview-project")
    set_tests_properties(component_preview_gpu PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE)
    if(RHYTHM_BUILD_MEDIA)
        add_executable(export_ui_gpu_tests tests/export_ui_gpu_tests.cpp)
        add_executable(soundtrack_studio_gpu_tests tests/soundtrack_studio_gpu_tests.cpp)
        target_link_libraries(soundtrack_studio_gpu_tests PRIVATE studio_ui project_io runtime_package spike_bgfx spike_imgui)
        rhythm_project_target(soundtrack_studio_gpu_tests)
        add_dependencies(soundtrack_studio_gpu_tests rhythm_master)
        add_test(NAME soundtrack_studio_gpu COMMAND soundtrack_studio_gpu_tests
            "${CMAKE_CURRENT_BINARY_DIR}" "${PROJECT_BINARY_DIR}/content/templates/resonance_live"
            "${PROJECT_BINARY_DIR}/music-fixtures/resonance_demo.wav" "${PROJECT_BINARY_DIR}/soundtrack-studio")
        set_tests_properties(soundtrack_studio_gpu PROPERTIES FIXTURES_REQUIRED music_pcm TIMEOUT 60 RUN_SERIAL TRUE)
        add_test(NAME large_soundtrack_studio_gpu COMMAND soundtrack_studio_gpu_tests
            "${CMAKE_CURRENT_BINARY_DIR}" "${PROJECT_BINARY_DIR}/content/templates/resonance_live"
            "${PROJECT_BINARY_DIR}/large-music-fixtures/resonance_demo.wav" "${PROJECT_BINARY_DIR}/large-soundtrack-studio")
        set_tests_properties(large_soundtrack_studio_gpu PROPERTIES FIXTURES_REQUIRED large_music_pcm TIMEOUT 60 RUN_SERIAL TRUE)
        add_test(NAME time_section_studio_gpu COMMAND soundtrack_studio_gpu_tests
            "${CMAKE_CURRENT_BINARY_DIR}" "${PROJECT_BINARY_DIR}/content/templates/resonance_live"
            "${PROJECT_BINARY_DIR}/large-music-fixtures/resonance_demo.wav" "${PROJECT_BINARY_DIR}/time-section-studio" --sections)
        set_tests_properties(time_section_studio_gpu PROPERTIES FIXTURES_REQUIRED large_music_pcm TIMEOUT 60 RUN_SERIAL TRUE)
        target_link_libraries(export_ui_gpu_tests PRIVATE studio_ui platform_sdl media_video project_io spike_bgfx spike_imgui)
        rhythm_project_target(export_ui_gpu_tests)
        add_dependencies(export_ui_gpu_tests studio_deploy)
        add_test(NAME export_ui_gpu COMMAND export_ui_gpu_tests
            "${CMAKE_CURRENT_BINARY_DIR}" "${PROJECT_BINARY_DIR}/content/templates/resonance_live"
            "${PROJECT_BINARY_DIR}/music-fixtures/resonance_demo.wav" "${PROJECT_BINARY_DIR}/export-ui")
        set_tests_properties(export_ui_gpu PROPERTIES FIXTURES_REQUIRED music_pcm TIMEOUT 120 RUN_SERIAL TRUE)
        add_executable(export_gpu_tests tests/export_gpu_tests.cpp)
        target_link_libraries(export_gpu_tests PRIVATE export_core platform_sdl media_video)
        rhythm_project_target(export_gpu_tests)
        add_dependencies(export_gpu_tests runtime_builtin_content)
        add_test(NAME export_gpu COMMAND export_gpu_tests
            "${PROJECT_BINARY_DIR}/content/packages/resonance_live.rhythmpack"
            "${PROJECT_BINARY_DIR}/music-fixtures/resonance_demo.wav"
            "${PROJECT_BINARY_DIR}/export-gpu")
        set_tests_properties(export_gpu PROPERTIES FIXTURES_REQUIRED music_pcm TIMEOUT 120 RUN_SERIAL TRUE)
        add_executable(audio_transport_tests tests/audio_transport_tests.cpp)
        add_executable(music_import_tests tests/music_import_tests.cpp
            "${PROJECT_SOURCE_DIR}/src/android_player/music_playback.cpp")
        target_include_directories(music_import_tests PRIVATE "${PROJECT_SOURCE_DIR}/src/android_player")
        target_link_libraries(music_import_tests PRIVATE audio_playback runtime_inputs media_types)
        rhythm_project_target(music_import_tests)
        target_link_libraries(audio_transport_tests PRIVATE audio_ui player_core)
        rhythm_project_target(audio_transport_tests)
        add_dependencies(audio_transport_tests demo_audio_content)
        add_test(NAME windows_studio_music_smoke
            COMMAND "$<TARGET_FILE_DIR:rhythm_master>/deploy/rhythm_master.exe" --smoke
                --audio "${PROJECT_BINARY_DIR}/content/audio/resonance_demo.wav")
        set_tests_properties(windows_studio_music_smoke PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE)
        add_executable(music_gpu_tests tests/music_gpu_tests.cpp)
        target_link_libraries(music_gpu_tests PRIVATE platform_sdl spike_bgfx player_core media_audio audio_analysis)
        rhythm_project_target(music_gpu_tests)
        add_dependencies(music_gpu_tests runtime_builtin_content)
        add_dependencies(music_gpu_tests performance_workflow_tests)
        add_test(NAME music_fixtures COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/create-music-fixture.py"
            --output "${PROJECT_BINARY_DIR}/music-fixtures" --tones)
        set_tests_properties(music_fixtures PROPERTIES FIXTURES_SETUP music_pcm TIMEOUT 30)
        add_test(NAME music_gpu COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/test-music-gpu.py"
            --executable "$<TARGET_FILE:music_gpu_tests>" --package "${PROJECT_BINARY_DIR}/content/packages/resonance_gate.rhythmpack"
            --fixtures "${PROJECT_BINARY_DIR}/music-fixtures" --output "${PROJECT_BINARY_DIR}/music-gpu-captures")
        set_tests_properties(music_gpu PROPERTIES FIXTURES_REQUIRED music_pcm TIMEOUT 120 RUN_SERIAL TRUE)
        add_test(NAME resonance_live_music_gpu COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/test-music-gpu.py"
            --executable "$<TARGET_FILE:music_gpu_tests>" --package "${PROJECT_BINARY_DIR}/content/packages/resonance_live.rhythmpack"
            --expected-nodes 197 --fixtures "${PROJECT_BINARY_DIR}/music-fixtures"
            --output "${PROJECT_BINARY_DIR}/resonance-live-music-captures")
        set_tests_properties(resonance_live_music_gpu PROPERTIES FIXTURES_REQUIRED music_pcm TIMEOUT 120 RUN_SERIAL TRUE)
        add_test(NAME performance_music_gpu COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/test-music-gpu.py"
            --executable "$<TARGET_FILE:music_gpu_tests>" --package "${PROJECT_BINARY_DIR}/performance-workflow/performance.rhythmpack"
            --expected-nodes 197 --fixtures "${PROJECT_BINARY_DIR}/music-fixtures"
            --output "${PROJECT_BINARY_DIR}/performance-music-captures")
        set_tests_properties(performance_music_gpu PROPERTIES FIXTURES_REQUIRED "performance_project;music_pcm"
            FIXTURES_SETUP performance_pixels TIMEOUT 120 RUN_SERIAL TRUE)
        add_test(NAME unpacked_music_gpu COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/test-music-gpu.py"
            --executable "$<TARGET_FILE:music_gpu_tests>" --package "${PROJECT_BINARY_DIR}/performance-workflow/unpacked-performance.rhythmpack"
            --expected-nodes 197 --fixtures "${PROJECT_BINARY_DIR}/music-fixtures"
            --output "${PROJECT_BINARY_DIR}/unpacked-music-captures" --reference-output "${PROJECT_BINARY_DIR}/performance-music-captures")
        set_tests_properties(unpacked_music_gpu PROPERTIES FIXTURES_REQUIRED performance_pixels TIMEOUT 120 RUN_SERIAL TRUE)
        add_executable(video_gpu_tests tests/video_gpu_tests.cpp)
        target_link_libraries(video_gpu_tests PRIVATE platform_sdl spike_bgfx graph_runtime video_sources asset_store)
        rhythm_project_target(video_gpu_tests)
        add_test(NAME video_gpu COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/test-video-gpu.py"
            --executable "$<TARGET_FILE:video_gpu_tests>" --fixtures "${PROJECT_BINARY_DIR}/video-playback-fixtures"
            --output "${PROJECT_BINARY_DIR}/video-gpu-captures")
        set_tests_properties(video_gpu PROPERTIES FIXTURES_REQUIRED video_playback_media TIMEOUT 90 RUN_SERIAL TRUE)
        add_executable(image_gpu_tests tests/image_gpu_tests.cpp)
        target_link_libraries(image_gpu_tests PRIVATE platform_sdl spike_bgfx player_core asset_store)
        rhythm_project_target(image_gpu_tests)
        add_test(NAME image_gpu COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/test-image-gpu.py"
            --executable "$<TARGET_FILE:image_gpu_tests>" --output "${PROJECT_BINARY_DIR}/image-gpu-captures")
        set_tests_properties(image_gpu PROPERTIES TIMEOUT 90 RUN_SERIAL TRUE)
    endif()
    add_executable(catalog_gpu_tests tests/catalog_gpu_tests.cpp)
    target_include_directories(catalog_gpu_tests PRIVATE "${PROJECT_SOURCE_DIR}/src/studio_ui")
    target_link_libraries(catalog_gpu_tests PRIVATE studio_ui player_core project_io spike_imgui spike_bgfx nlohmann_json::nlohmann_json)
    rhythm_project_target(catalog_gpu_tests)
    add_dependencies(catalog_gpu_tests studio_resources)
    add_test(NAME catalog_gpu COMMAND catalog_gpu_tests "${CMAKE_CURRENT_BINARY_DIR}"
        "${PROJECT_BINARY_DIR}/catalog-review")
    set_tests_properties(catalog_gpu PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE)
    add_executable(template_text_gpu_tests tests/template_text_gpu_tests.cpp)
    target_link_libraries(template_text_gpu_tests PRIVATE platform_sdl spike_imgui project_io spike_bgfx)
    rhythm_project_target(template_text_gpu_tests)
    add_dependencies(template_text_gpu_tests phase_a_content)
    add_test(NAME template_text_gpu COMMAND template_text_gpu_tests "${PROJECT_BINARY_DIR}/content/templates"
        "${PROJECT_BINARY_DIR}/template-text-review")
    set_tests_properties(template_text_gpu PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE)
    add_executable(windows_effects_gpu_tests tests/effects_gpu_tests.cpp)
    target_include_directories(windows_effects_gpu_tests PRIVATE "${PROJECT_SOURCE_DIR}/src/runtime")
    target_link_libraries(windows_effects_gpu_tests PRIVATE platform_sdl spike_bgfx graph_runtime player_core)
    rhythm_project_target(windows_effects_gpu_tests)
    add_test(NAME windows_effects_gpu COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/test-effects-gpu.py"
        --executable "$<TARGET_FILE:windows_effects_gpu_tests>" --output "${PROJECT_BINARY_DIR}/effects-captures")
    set_tests_properties(windows_effects_gpu PROPERTIES TIMEOUT 90 RUN_SERIAL TRUE)
    add_executable(windows_scene_gpu_tests tests/scene_gpu_tests.cpp)
    target_include_directories(windows_scene_gpu_tests PRIVATE "${PROJECT_SOURCE_DIR}/src/rhythm_render/tests")
    target_include_directories(windows_scene_gpu_tests PRIVATE "${PROJECT_SOURCE_DIR}/src/runtime/tests")
    target_link_libraries(windows_scene_gpu_tests PRIVATE platform_sdl spike_bgfx graph_runtime runtime_package player_core asset_store)
    rhythm_project_target(windows_scene_gpu_tests)
    add_test(NAME windows_scene_gpu COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/test-scene-gpu.py"
        --executable "$<TARGET_FILE:windows_scene_gpu_tests>" --output "${PROJECT_BINARY_DIR}/scene-captures")
    set_tests_properties(windows_scene_gpu PROPERTIES TIMEOUT 90 RUN_SERIAL TRUE)
    add_test(NAME windows_deploy_smoke COMMAND "${Python3_EXECUTABLE}"
        "${PROJECT_SOURCE_DIR}/tools/test-deploy.py"
        --executable "$<TARGET_FILE_DIR:rhythm_master>/deploy/$<TARGET_FILE_NAME:rhythm_master>"
        --output-directory "${PROJECT_BINARY_DIR}/deployment-test")
    set_tests_properties(windows_deploy_smoke PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE)
    include("${PROJECT_SOURCE_DIR}/cmake/GraphicsProbes.cmake")
    add_executable(host_contract_tests tests/host_contract_tests.cpp)
    target_link_libraries(host_contract_tests PRIVATE platform_sdl)
    rhythm_project_target(host_contract_tests)
    add_test(NAME windows_host_contracts COMMAND host_contract_tests)
    add_test(NAME windows_gpu_smoke COMMAND rhythm_master --smoke)
    set_tests_properties(windows_gpu_smoke windows_host_contracts PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${RHYTHM_SPIKE_SDK}/bin;PATH=path_list_prepend:${RHYTHM_SPIKE_SDK}/debug/bin")
endif()
