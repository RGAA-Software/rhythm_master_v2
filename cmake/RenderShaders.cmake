set(RHYTHM_SHADERC "" CACHE FILEPATH "Host bgfx shaderc executable")
if(NOT EXISTS "${RHYTHM_SHADERC}")
    message(FATAL_ERROR "Configure RHYTHM_SHADERC with the validated host shaderc executable")
endif()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(render_shader_platform windows)
if(ANDROID)
    set(render_shader_platform android)
endif()
set(render_shader_header "${PROJECT_BINARY_DIR}/generated/render/color_adjust_shader.h")
file(GLOB render_shader_includes "${PROJECT_SOURCE_DIR}/third_party/sources/bgfx/src/*.sh")
add_custom_command(OUTPUT "${render_shader_header}"
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/build-render-shaders.py"
        --compiler "${RHYTHM_SHADERC}" --output "${render_shader_header}"
        --platform "${render_shader_platform}"
    DEPENDS "${PROJECT_SOURCE_DIR}/tools/build-render-shaders.py"
        "${PROJECT_SOURCE_DIR}/tools/compile-shader.py"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/shaders/color_adjust.sc"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/shaders/varying.def.sc"
        "${RHYTHM_SHADERC}" ${render_shader_includes}
    VERBATIM)
target_sources(render_bgfx PRIVATE "${render_shader_header}")

set(filter_shader_header "${PROJECT_BINARY_DIR}/generated/render/texture_filter_shader.h")
add_custom_command(OUTPUT "${filter_shader_header}"
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/build-render-shaders.py"
        --compiler "${RHYTHM_SHADERC}" --output "${filter_shader_header}"
        --platform "${render_shader_platform}" --group filter
    DEPENDS "${PROJECT_SOURCE_DIR}/tools/build-render-shaders.py"
        "${PROJECT_SOURCE_DIR}/tools/compile-shader.py"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/shaders/texture_filter.sc"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/shaders/varying.def.sc"
        "${RHYTHM_SHADERC}" ${render_shader_includes}
    VERBATIM)
target_sources(render_bgfx PRIVATE "${filter_shader_header}")

set(noise_shader_header "${PROJECT_BINARY_DIR}/generated/render/texture_noise_shader.h")
add_custom_command(OUTPUT "${noise_shader_header}"
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/build-render-shaders.py"
        --compiler "${RHYTHM_SHADERC}" --output "${noise_shader_header}"
        --platform "${render_shader_platform}" --group noise
    DEPENDS "${PROJECT_SOURCE_DIR}/tools/build-render-shaders.py"
        "${PROJECT_SOURCE_DIR}/tools/compile-shader.py"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/shaders/texture_noise.sc"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/shaders/varying.def.sc"
        "${RHYTHM_SHADERC}" ${render_shader_includes}
    VERBATIM)
target_sources(render_bgfx PRIVATE "${noise_shader_header}")
target_include_directories(render_bgfx PRIVATE "${PROJECT_BINARY_DIR}/generated/render")
set(scene_shader_header "${PROJECT_BINARY_DIR}/generated/render/scene_shader.h")
add_custom_command(OUTPUT "${scene_shader_header}"
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/build-render-shaders.py"
        --compiler "${RHYTHM_SHADERC}" --output "${scene_shader_header}"
        --platform "${render_shader_platform}" --group scene
    DEPENDS "${PROJECT_SOURCE_DIR}/tools/build-render-shaders.py"
        "${PROJECT_SOURCE_DIR}/tools/compile-shader.py"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/shaders/scene_vertex.sc"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/shaders/scene_fragment.sc"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/shaders/godot_brdf.sh"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/shaders/scene_varying.def.sc"
        "${RHYTHM_SHADERC}" ${render_shader_includes}
    VERBATIM)
target_sources(render_bgfx PRIVATE "${scene_shader_header}"
    "${PROJECT_SOURCE_DIR}/src/rhythm_render/src/bgfx_scene.cpp")

target_sources(render_bgfx PRIVATE "${PROJECT_SOURCE_DIR}/src/rhythm_render/src/bgfx_texture_programs.cpp")
set(mapping_shader_header "${PROJECT_BINARY_DIR}/generated/render/texture_mapping_shader.h")
add_custom_command(OUTPUT "${mapping_shader_header}"
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/build-render-shaders.py"
        --compiler "${RHYTHM_SHADERC}" --output "${mapping_shader_header}"
        --platform "${render_shader_platform}" --group mapping
    DEPENDS "${PROJECT_SOURCE_DIR}/tools/build-render-shaders.py"
        "${PROJECT_SOURCE_DIR}/tools/compile-shader.py"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/shaders/texture_mapping.sc"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/shaders/texture_contours.sc"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/shaders/varying.def.sc"
        "${RHYTHM_SHADERC}" ${render_shader_includes}
    VERBATIM)
target_sources(render_bgfx PRIVATE "${mapping_shader_header}")

set(displace_shader_header "${PROJECT_BINARY_DIR}/generated/render/texture_displace_shader.h")
add_custom_command(OUTPUT "${displace_shader_header}"
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/build-render-shaders.py"
        --compiler "${RHYTHM_SHADERC}" --output "${displace_shader_header}"
        --platform "${render_shader_platform}" --group displace
    DEPENDS "${PROJECT_SOURCE_DIR}/tools/build-render-shaders.py"
        "${PROJECT_SOURCE_DIR}/tools/compile-shader.py"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/shaders/texture_displace.sc"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/shaders/texture_trail.sc"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/shaders/varying.def.sc"
        "${RHYTHM_SHADERC}" ${render_shader_includes}
    VERBATIM)
target_sources(render_bgfx PRIVATE "${displace_shader_header}")
