find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(surface_header "${PROJECT_BINARY_DIR}/generated/surface/surface_template.h")
add_custom_command(OUTPUT "${surface_header}"
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/surface_template.py"
        --output "${surface_header}"
    DEPENDS "${PROJECT_SOURCE_DIR}/tools/surface_template.py"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/shaders/scene_fragment.sc"
    VERBATIM)
add_library(surface_shader STATIC source.cpp program.cpp "${surface_header}")
target_include_directories(surface_shader PUBLIC include PRIVATE "${PROJECT_BINARY_DIR}/generated/surface")
target_link_libraries(surface_shader PUBLIC shader_expression shader_artifact)
rhythm_project_target(surface_shader)
if(BUILD_TESTING)
    add_executable(surface_shader_tests tests/program_tests.cpp)
    target_link_libraries(surface_shader_tests PRIVATE surface_shader image_shader)
    rhythm_project_target(surface_shader_tests)
endif()
