add_library(render_bgfx STATIC ../rhythm_render/src/bgfx_backend.cpp)
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
add_subdirectory(../studio_ui "${PROJECT_BINARY_DIR}/src/studio_ui")
add_executable(rhythm_master main.cpp)
target_link_libraries(rhythm_master PRIVATE platform_sdl studio_ui graph_runtime)
rhythm_project_target(rhythm_master)
add_custom_target(studio_resources ALL
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/sync-resources.py"
        "${PROJECT_SOURCE_DIR}/content/presets" "${PROJECT_BINARY_DIR}/content/presets"
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/sync-resources.py"
        "${PROJECT_BINARY_DIR}/content" "${CMAKE_CURRENT_BINARY_DIR}/content"
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/sync-resources.py"
        "${PROJECT_SOURCE_DIR}/locales" "${CMAKE_CURRENT_BINARY_DIR}/locales"
    DEPENDS phase_a_content semantic_content runtime_builtin_content)
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
")
set(rhythm_deploy_command "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/deploy-windows.py"
    --config "${rhythm_deploy_config}")
add_custom_command(TARGET rhythm_master POST_BUILD COMMAND ${rhythm_deploy_command} VERBATIM)
# Also deploy on resource-only and no-op default builds.
add_custom_target(studio_deploy ALL COMMAND ${rhythm_deploy_command}
    DEPENDS rhythm_master studio_resources VERBATIM)
if(BUILD_TESTING)
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
