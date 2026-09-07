set(RHYTHM_SHADERC "" CACHE FILEPATH "Read-only shaderc candidate for native graphics validation")
if(RHYTHM_SHADERC)
    if(NOT EXISTS "${RHYTHM_SHADERC}")
        message(FATAL_ERROR "The configured shaderc candidate does not exist")
    endif()
    set(probe_shader "${CMAKE_CURRENT_BINARY_DIR}/probes/compute.bin")
    add_custom_command(OUTPUT "${probe_shader}"
        COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/tools/compile-shader.py"
            --compiler "${RHYTHM_SHADERC}"
            --source "${CMAKE_CURRENT_SOURCE_DIR}/tests/shaders/compute_probe.sc"
            --output "${probe_shader}" --stage compute
            --include "${rhythm_deps}/bgfx/src"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/shaders/compute_probe.sc"
            "${PROJECT_SOURCE_DIR}/tools/compile-shader.py" "${RHYTHM_SHADERC}"
            "${rhythm_deps}/bgfx/src/bgfx_compute.sh" "${rhythm_deps}/bgfx/src/bgfx_shader.sh"
        VERBATIM)
    add_custom_target(compute_probe_shader DEPENDS "${probe_shader}")
    add_executable(gpu_probe tests/gpu_probe.cpp)
    target_include_directories(gpu_probe PRIVATE ../rhythm_render/src)
    target_link_libraries(gpu_probe PRIVATE platform_sdl spike_bgfx)
    rhythm_project_target(gpu_probe)
    add_dependencies(gpu_probe compute_probe_shader)
    add_test(NAME shader_compiler_contracts COMMAND ${Python3_EXECUTABLE}
        "${PROJECT_SOURCE_DIR}/tools/test-shader-compiler.py" "${RHYTHM_SHADERC}")
    add_test(NAME windows_compute_probe COMMAND gpu_probe "${probe_shader}")
    set_tests_properties(windows_compute_probe PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${RHYTHM_SPIKE_SDK}/bin;PATH=path_list_prepend:${RHYTHM_SPIKE_SDK}/debug/bin")
endif()
