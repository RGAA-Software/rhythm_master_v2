cmake_minimum_required(VERSION 3.25)
project(RhythmMediaValidation LANGUAGES C CXX)
set(rhythm_root "${CMAKE_CURRENT_SOURCE_DIR}/../..")
if(MSVC AND DEFINED RHYTHM_MSVC_INCLUDE_PREFIX)
    set(CMAKE_CL_SHOWINCLUDES_PREFIX "${RHYTHM_MSVC_INCLUDE_PREFIX}")
endif()
include("${rhythm_root}/cmake/ProjectOptions.cmake")
set(BUILD_TESTING ON)
add_subdirectory("${rhythm_root}/src/media" media)
set(RHYTHM_SDL_BUILD "" CACHE PATH "Validated SDL build package; vcpkg SDL3 is not installed yet")
find_package(SDL3 CONFIG REQUIRED PATHS "${RHYTHM_SDL_BUILD}"
    NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
add_subdirectory("${rhythm_root}/src/audio_analysis" audio_analysis)
add_subdirectory("${rhythm_root}/src/audio_output" audio_output)
add_subdirectory("${rhythm_root}/src/audio_playback" audio_playback)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
if(WIN32)
    foreach(probe_target media_audio_tests audio_playback_tests audio_output_tests)
    file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/deploy-${probe_target}.txt" CONTENT
"executable=$<TARGET_FILE:${probe_target}>
runtime_dlls=$<TARGET_RUNTIME_DLLS:${probe_target}>
configuration=${CMAKE_BUILD_TYPE}
compiler=${CMAKE_CXX_COMPILER}
sdk=${RHYTHM_MEDIA_SDK}
io_sdk=
")
    add_custom_target(${probe_target}_deploy ALL
        COMMAND "${Python3_EXECUTABLE}" "${rhythm_root}/tools/deploy-media-probe.py"
            --config "${CMAKE_CURRENT_BINARY_DIR}/deploy-${probe_target}.txt"
        DEPENDS ${probe_target} VERBATIM)
    endforeach()
endif()
