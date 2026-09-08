add_executable(rhythm_player main.cpp)
target_sources(rhythm_player PRIVATE scene_queue_panel.cpp)
target_link_libraries(rhythm_player PRIVATE platform_sdl player_core project_io audio_ui control_ui spike_imgui nlohmann_json::nlohmann_json)
rhythm_project_target(rhythm_player)
add_dependencies(rhythm_player runtime_builtin_content)
set(player_deploy_config "${CMAKE_CURRENT_BINARY_DIR}/deploy-config-$<CONFIG>.txt")
file(GENERATE OUTPUT "${player_deploy_config}" CONTENT
"executable=$<TARGET_FILE:rhythm_player>
runtime_dlls=$<TARGET_RUNTIME_DLLS:rhythm_player>
configuration=$<CONFIG>
compiler=${CMAKE_CXX_COMPILER}
source_root=${PROJECT_SOURCE_DIR}
build_root=${PROJECT_BINARY_DIR}
sdk=${RHYTHM_SPIKE_SDK}
io_sdk=${RHYTHM_IO_SDK}
media_sdk=${RHYTHM_MEDIA_SDK}
")
set(player_deploy_command "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/deploy-windows.py" --config "${player_deploy_config}")
add_custom_command(TARGET rhythm_player POST_BUILD COMMAND ${player_deploy_command} VERBATIM)
add_custom_target(player_deploy ALL COMMAND ${player_deploy_command} DEPENDS rhythm_player runtime_builtin_content demo_audio_content VERBATIM)
add_dependencies(rhythm_player demo_audio_content)
if(BUILD_TESTING)
    add_executable(scene_queue_ui_tests scene_queue_ui_tests.cpp scene_queue_panel.cpp)
    target_link_libraries(scene_queue_ui_tests PRIVATE platform_sdl player_core spike_imgui nlohmann_json::nlohmann_json)
    rhythm_project_target(scene_queue_ui_tests)
    add_test(NAME scene_queue_ui COMMAND scene_queue_ui_tests
        "${PROJECT_SOURCE_DIR}"
        "${PROJECT_BINARY_DIR}/content/packages/crystal_choir.rhythmpack"
        "${PROJECT_BINARY_DIR}/content/packages/luminous_concerto.rhythmpack")
    set_tests_properties(scene_queue_ui PROPERTIES TIMEOUT 30 RUN_SERIAL TRUE
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${RHYTHM_IO_SDK}/bin;PATH=path_list_prepend:${RHYTHM_MEDIA_SDK}/bin")
    add_test(NAME windows_player_smoke COMMAND "$<TARGET_FILE_DIR:rhythm_player>/deploy/rhythm_player.exe" --smoke)
    set_tests_properties(windows_player_smoke PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE WORKING_DIRECTORY "${PROJECT_BINARY_DIR}")
endif()
