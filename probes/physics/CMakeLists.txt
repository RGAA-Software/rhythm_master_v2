cmake_minimum_required(VERSION 3.25)
project(RhythmPhysicsValidation LANGUAGES C CXX)
set(rhythm_root "${CMAKE_CURRENT_SOURCE_DIR}/../..")
if(MSVC AND DEFINED RHYTHM_MSVC_INCLUDE_PREFIX)
    set(CMAKE_CL_SHOWINCLUDES_PREFIX "${RHYTHM_MSVC_INCLUDE_PREFIX}")
endif()
include("${rhythm_root}/cmake/ProjectOptions.cmake")
include(CTest)
set(RHYTHM_PHYSICS_SDK "" CACHE PATH "Validated vcpkg Box2D SDK")
find_package(box2d 3.1.1 CONFIG REQUIRED PATHS "${RHYTHM_PHYSICS_SDK}/share/box2d"
    NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
add_subdirectory("${rhythm_root}/src/physics2d" physics2d)
target_sources(physics2d_tests PRIVATE distance_regression.cpp)
target_link_libraries(physics2d_tests PRIVATE box2d::box2d)
target_compile_definitions(physics2d_tests PRIVATE RHYTHM_NATIVE_PHYSICS_PROBE=1)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
if(WIN32)
    file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/deploy-physics.txt" CONTENT
"executable=$<TARGET_FILE:physics2d_tests>
runtime_dlls=$<TARGET_RUNTIME_DLLS:physics2d_tests>
configuration=${CMAKE_BUILD_TYPE}
compiler=${CMAKE_CXX_COMPILER}
sdk=${RHYTHM_PHYSICS_SDK}
io_sdk=
packages=box2d
")
    add_custom_target(physics_probe_deploy ALL
        COMMAND "${Python3_EXECUTABLE}" "${rhythm_root}/tools/deploy-probe.py"
            --config "${CMAKE_CURRENT_BINARY_DIR}/deploy-physics.txt"
        DEPENDS physics2d_tests VERBATIM)
endif()
