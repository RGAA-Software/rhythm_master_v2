function(rhythm_project_target target)
    target_compile_features(${target} PUBLIC cxx_std_20)
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
    get_target_property(rhythm_target_type ${target} TYPE)
    if(WIN32 AND rhythm_target_type STREQUAL "EXECUTABLE")
        find_package(Python3 REQUIRED COMPONENTS Interpreter)
        set(rhythm_runtime_config "${CMAKE_CURRENT_BINARY_DIR}/${target}-runtime-$<CONFIG>.txt")
        file(GENERATE OUTPUT "${rhythm_runtime_config}" CONTENT
            "$<TARGET_FILE:${target}>\n$<TARGET_RUNTIME_DLLS:${target}>\n")
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../tools/copy-linked-runtime.py"
                --config "${rhythm_runtime_config}" VERBATIM)
    endif()
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /permissive- /utf-8 /Zc:__cplusplus /Zc:preprocessor)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endfunction()
