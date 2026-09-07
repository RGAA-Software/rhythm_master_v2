cmake_minimum_required(VERSION 3.25)
project(RhythmQuicProbe LANGUAGES CXX)
if(MSVC AND DEFINED RHYTHM_MSVC_INCLUDE_PREFIX)
    set(CMAKE_CL_SHOWINCLUDES_PREFIX "${RHYTHM_MSVC_INCLUDE_PREFIX}")
    add_compile_options(/FC)
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/ProjectOptions.cmake")
set(MSQUIC_SOURCE "" CACHE PATH "Pinned candidate source")
set(MSQUIC_LIBRARY "" CACHE FILEPATH "Candidate import/shared library")
set(MSQUIC_RUNTIME "" CACHE FILEPATH "Windows candidate DLL")
option(QUIC_PROBE_OPENSSL "Probe the OpenSSL TLS provider" OFF)
set(QUIC_PROBE_TLS_ROOT "" CACHE PATH "Shared TLS candidate deployment root")
if(MSQUIC_LIBRARY)
add_library(quic_probe_buffers STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/../../src/cluster/send_queue.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../src/cluster/stream_decoder.cpp")
target_include_directories(quic_probe_buffers PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../../src/cluster/include")
rhythm_project_target(quic_probe_buffers)
add_executable(msquic_probe msquic_probe.cpp stress.cpp metrics.cpp flow_control.cpp)
target_include_directories(msquic_probe SYSTEM PRIVATE "${MSQUIC_SOURCE}/src/inc")
target_link_libraries(msquic_probe PRIVATE "${MSQUIC_LIBRARY}" quic_probe_buffers)
if(QUIC_PROBE_OPENSSL)
    target_compile_definitions(msquic_probe PRIVATE RHYTHM_QUIC_OPENSSL)
endif()
if(WIN32)
    target_link_libraries(msquic_probe PRIVATE ws2_32 ntdll crypt32 psapi)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_custom_command(TARGET msquic_probe POST_BUILD
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/../../tools/deploy-quic-probe.py"
            "$<TARGET_FILE:msquic_probe>" "${MSQUIC_RUNTIME}" "${CMAKE_CXX_COMPILER}"
            "$<BOOL:${QUIC_PROBE_OPENSSL}>" "${QUIC_PROBE_TLS_ROOT}")
    add_custom_target(deploy_quic_probe ALL
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/../../tools/deploy-quic-probe.py"
            "$<TARGET_FILE:msquic_probe>" "${MSQUIC_RUNTIME}" "${CMAKE_CXX_COMPILER}"
            "$<BOOL:${QUIC_PROBE_OPENSSL}>" "${QUIC_PROBE_TLS_ROOT}"
        DEPENDS msquic_probe)
endif()
rhythm_project_target(msquic_probe)
endif()
set(QUICHE_SOURCE "" CACHE PATH "Pinned quiche source")
set(QUICHE_LIBRARY "" CACHE FILEPATH "quiche import/shared library")
set(QUICHE_RUNTIME "" CACHE FILEPATH "Windows quiche DLL")
if(QUICHE_LIBRARY)
    add_executable(quiche_probe quiche_probe.cpp)
    target_include_directories(quiche_probe SYSTEM PRIVATE "${QUICHE_SOURCE}/quiche/include")
    target_link_libraries(quiche_probe PRIVATE "${QUICHE_LIBRARY}")
    rhythm_project_target(quiche_probe)
    if(WIN32)
        target_link_libraries(quiche_probe PRIVATE ws2_32)
        find_package(Python3 REQUIRED COMPONENTS Interpreter)
        add_custom_command(TARGET quiche_probe POST_BUILD
            COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/../../tools/deploy-quic-probe.py"
                "$<TARGET_FILE:quiche_probe>" "${QUICHE_RUNTIME}" "${CMAKE_CXX_COMPILER}" quiche)
        add_custom_target(deploy_quiche_probe ALL
            COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/../../tools/deploy-quic-probe.py"
                "$<TARGET_FILE:quiche_probe>" "${QUICHE_RUNTIME}" "${CMAKE_CXX_COMPILER}" quiche
            DEPENDS quiche_probe)
    endif()
endif()
