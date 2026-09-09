add_library(shader_authoring STATIC compiler.cpp process.cpp)
target_include_directories(shader_authoring PUBLIC include
    PRIVATE "${PROJECT_SOURCE_DIR}/third_party/sources/picosha2")
target_link_libraries(shader_authoring PUBLIC asset_store image_shader foundation_async
    PRIVATE SDL3::SDL3-static surface_shader)
rhythm_project_target(shader_authoring)
add_executable(shader_author_tool tool.cpp)
target_link_libraries(shader_author_tool PRIVATE shader_authoring)
rhythm_project_target(shader_author_tool)
if(BUILD_TESTING)
    add_executable(shader_authoring_tests tests/compiler_tests.cpp)
    target_link_libraries(shader_authoring_tests PRIVATE shader_authoring prepared_assets project_io)
    rhythm_project_target(shader_authoring_tests)
    add_test(NAME shader_authoring COMMAND shader_authoring_tests "${RHYTHM_SHADERC}"
        "${PROJECT_SOURCE_DIR}/third_party/sources/bgfx/src"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/shaders/varying.def.sc"
        "${PROJECT_BINARY_DIR}/shader-authoring-tests")
    set_tests_properties(shader_authoring PROPERTIES TIMEOUT 90 RUN_SERIAL TRUE ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${RHYTHM_MEDIA_SDK}/bin;PATH=path_list_prepend:${RHYTHM_IO_SDK}/bin;PATH=path_list_prepend:${RHYTHM_SPIKE_SDK}/bin")
endif()
