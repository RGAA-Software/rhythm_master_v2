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
target_sources(render_bgfx PRIVATE "${PROJECT_SOURCE_DIR}/src/rhythm_render/src/bgfx_image_programs.cpp")
target_link_libraries(render_bgfx PRIVATE image_shader)

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
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/shaders/mesh_deformation.sh"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/shaders/mesh_skinning.sh"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/shaders/mesh_morph.sh"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/shaders/scene_morph_vertex.sc"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/shaders/scene_morph_instance.sc"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/shaders/scene_vertex_body.sh"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/shaders/scene_instance_body.sh"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/shaders/scene_skin_vertex.sc"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/shaders/scene_skin_instance.sc"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/shaders/scene_instance.sc"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/shaders/scene_fragment.sc"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/shaders/godot_brdf.sh"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/shaders/godot_lights.sh"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/shaders/godot_shadow.sh"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/shaders/godot_environment.sh"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/shaders/scene_varying.def.sc"
        "${RHYTHM_SHADERC}" ${render_shader_includes}
    VERBATIM)
target_sources(render_bgfx PRIVATE "${scene_shader_header}"
    "${PROJECT_SOURCE_DIR}/src/rhythm_render/src/bgfx_scene.cpp"
    "${PROJECT_SOURCE_DIR}/src/rhythm_render/src/bgfx_scene_instances.cpp")
target_sources(render_bgfx PRIVATE "${PROJECT_SOURCE_DIR}/src/rhythm_render/src/bgfx_scene_lights.cpp")
target_sources(render_bgfx PRIVATE "${PROJECT_SOURCE_DIR}/src/rhythm_render/src/bgfx_scene_textures.cpp")
target_sources(render_bgfx PRIVATE "${PROJECT_SOURCE_DIR}/src/rhythm_render/src/bgfx_scene_shadow.cpp")
target_sources(render_bgfx PRIVATE "${PROJECT_SOURCE_DIR}/src/rhythm_render/src/bgfx_scene_skin.cpp")
target_sources(render_bgfx PRIVATE "${PROJECT_SOURCE_DIR}/src/rhythm_render/src/bgfx_scene_morph.cpp")

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

if(BUILD_TESTING)
    set(probe_shader_header "${PROJECT_BINARY_DIR}/generated/render/gpu_execution_probe_shader.h")
    add_custom_command(OUTPUT "${probe_shader_header}"
        COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/build-render-shaders.py"
            --compiler "${RHYTHM_SHADERC}" --output "${probe_shader_header}"
            --platform "${render_shader_platform}" --group execution_probe
        DEPENDS "${PROJECT_SOURCE_DIR}/tools/build-render-shaders.py"
            "${PROJECT_SOURCE_DIR}/tools/compile-shader.py"
            "${PROJECT_SOURCE_DIR}/src/rhythm_render/shaders/probe_instance.sc"
            "${PROJECT_SOURCE_DIR}/src/rhythm_render/shaders/probe_color.sc"
            "${PROJECT_SOURCE_DIR}/src/rhythm_render/shaders/probe_update.sc"
            "${PROJECT_SOURCE_DIR}/src/rhythm_render/shaders/probe_varying.def.sc"
            "${RHYTHM_SHADERC}" ${render_shader_includes}
        VERBATIM)
    add_library(gpu_execution_probe STATIC
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/tests/quality_baseline_gpu.cpp"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/tests/fxaa_gpu.cpp"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/tests/color_pipeline_gpu.cpp"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/tests/depth_gpu.cpp"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/tests/lights_gpu.cpp"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/tests/material_textures_gpu.cpp"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/tests/shadows_gpu.cpp"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/tests/environment_gpu.cpp"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/tests/deformation_gpu.cpp"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/tests/skinning_gpu.cpp"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/tests/morph_gpu.cpp"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/tests/image_program_gpu.cpp"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/tests/gpu_particles_gpu.cpp"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/tests/scene_instances_gpu.cpp"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/tests/gpu_execution_probe.cpp" "${probe_shader_header}")
    target_include_directories(gpu_execution_probe PRIVATE
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/src" "${PROJECT_BINARY_DIR}/generated/render")
    target_include_directories(gpu_execution_probe PUBLIC "${PROJECT_SOURCE_DIR}/src/rhythm_render/tests")
    target_link_libraries(gpu_execution_probe PRIVATE Rhythm::Render spike_bgfx)
    target_include_directories(gpu_execution_probe SYSTEM PRIVATE "${RHYTHM_GLM_INCLUDE}")
    if(RHYTHM_BUILD_MEDIA)
        target_sources(gpu_execution_probe PRIVATE "${PROJECT_SOURCE_DIR}/src/rhythm_render/tests/model_images_gpu.cpp")
        target_sources(gpu_execution_probe PRIVATE "${PROJECT_SOURCE_DIR}/src/rhythm_render/tests/model_skin_gpu.cpp")
        target_sources(gpu_execution_probe PRIVATE "${PROJECT_SOURCE_DIR}/src/rhythm_render/tests/model_morph_gpu.cpp")
        target_include_directories(gpu_execution_probe PRIVATE
            "${PROJECT_SOURCE_DIR}/src/model_import/tests" "${PROJECT_SOURCE_DIR}/third_party/sources/picosha2")
        target_link_libraries(gpu_execution_probe PRIVATE model_assets graph_runtime)
        target_compile_definitions(gpu_execution_probe PUBLIC RHYTHM_MODEL_IMAGE_PROBE=1)
    endif()
    rhythm_project_target(gpu_execution_probe)
endif()

set(gpu_point_shader_header "${PROJECT_BINARY_DIR}/generated/render/gpu_point_shader.h")
add_custom_command(OUTPUT "${gpu_point_shader_header}"
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/build-render-shaders.py"
        --compiler "${RHYTHM_SHADERC}" --output "${gpu_point_shader_header}"
        --platform "${render_shader_platform}" --group gpu_points
    DEPENDS "${PROJECT_SOURCE_DIR}/tools/build-render-shaders.py"
        "${PROJECT_SOURCE_DIR}/tools/compile-shader.py"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/shaders/gpu_point_vertex.sc"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/shaders/gpu_point_fragment.sc"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/shaders/gpu_particle_update.sc"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/shaders/gpu_point_varying.def.sc"
        "${RHYTHM_SHADERC}" ${render_shader_includes}
    VERBATIM)
target_sources(render_bgfx PRIVATE "${gpu_point_shader_header}"
    "${PROJECT_SOURCE_DIR}/src/rhythm_render/src/bgfx_gpu_points.cpp")

set(color_pipeline_shader_header "${PROJECT_BINARY_DIR}/generated/render/color_pipeline_shader.h")
add_custom_command(OUTPUT "${color_pipeline_shader_header}"
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/build-render-shaders.py"
        --compiler "${RHYTHM_SHADERC}" --output "${color_pipeline_shader_header}"
        --platform "${render_shader_platform}" --group color_pipeline
    DEPENDS "${PROJECT_SOURCE_DIR}/tools/build-render-shaders.py"
        "${PROJECT_SOURCE_DIR}/tools/compile-shader.py"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/shaders/color_pipeline.sc"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/shaders/varying.def.sc"
        "${RHYTHM_SHADERC}" ${render_shader_includes}
    VERBATIM)
target_sources(render_bgfx PRIVATE "${color_pipeline_shader_header}")

set(fxaa_shader_header "${PROJECT_BINARY_DIR}/generated/render/texture_fxaa_shader.h")
add_custom_command(OUTPUT "${fxaa_shader_header}"
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/build-render-shaders.py"
        --compiler "${RHYTHM_SHADERC}" --output "${fxaa_shader_header}"
        --platform "${render_shader_platform}" --group antialias
    DEPENDS "${PROJECT_SOURCE_DIR}/tools/build-render-shaders.py"
        "${PROJECT_SOURCE_DIR}/tools/compile-shader.py"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/shaders/texture_fxaa.sc"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/shaders/varying.def.sc"
        "${RHYTHM_SHADERC}" ${render_shader_includes}
    VERBATIM)
target_sources(render_bgfx PRIVATE "${fxaa_shader_header}")

set(depth_shader_header "${PROJECT_BINARY_DIR}/generated/render/depth_shader.h")
add_custom_command(OUTPUT "${depth_shader_header}"
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/build-render-shaders.py"
        --compiler "${RHYTHM_SHADERC}" --output "${depth_shader_header}"
        --platform "${render_shader_platform}" --group depth
    DEPENDS "${PROJECT_SOURCE_DIR}/tools/build-render-shaders.py"
        "${PROJECT_SOURCE_DIR}/tools/compile-shader.py"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/shaders/depth_linear.sc"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/shaders/depth_of_field.sc"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/shaders/varying.def.sc"
        "${RHYTHM_SHADERC}" ${render_shader_includes}
    VERBATIM)
target_sources(render_bgfx PRIVATE "${depth_shader_header}")

set(environment_shader_header "${PROJECT_BINARY_DIR}/generated/render/environment_shader.h")
add_custom_command(OUTPUT "${environment_shader_header}"
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/build-render-shaders.py"
        --compiler "${RHYTHM_SHADERC}" --output "${environment_shader_header}"
        --platform "${render_shader_platform}" --group environment
    DEPENDS "${PROJECT_SOURCE_DIR}/tools/build-render-shaders.py"
        "${PROJECT_SOURCE_DIR}/tools/compile-shader.py"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/shaders/environment_filter.sc"
        "${PROJECT_SOURCE_DIR}/src/rhythm_render/shaders/varying.def.sc"
        "${RHYTHM_SHADERC}" ${render_shader_includes}
    VERBATIM)
target_sources(render_bgfx PRIVATE "${environment_shader_header}"
    "${PROJECT_SOURCE_DIR}/src/rhythm_render/src/bgfx_scene_environment.cpp")
