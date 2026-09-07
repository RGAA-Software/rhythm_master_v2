# Compile the pinned candidate privately; package logic remains in C++/Python.
set(rhythm_miniz "${PROJECT_SOURCE_DIR}/third_party/sources/miniz")
if(NOT EXISTS "${rhythm_miniz}/miniz.c")
    message(FATAL_ERROR "Run python tools/prepare-miniz.py before building runtime packages")
endif()
add_library(rhythm_miniz STATIC "${rhythm_miniz}/miniz.c" "${rhythm_miniz}/miniz_zip.c"
    "${rhythm_miniz}/miniz_tdef.c" "${rhythm_miniz}/miniz_tinfl.c")
set_target_properties(rhythm_miniz PROPERTIES POSITION_INDEPENDENT_CODE ON)
include(GenerateExportHeader)
generate_export_header(rhythm_miniz EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/miniz_export.h"
    EXPORT_MACRO_NAME MINIZ_EXPORT)
target_include_directories(rhythm_miniz SYSTEM PUBLIC "${rhythm_miniz}" "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_definitions(rhythm_miniz PUBLIC MINIZ_NO_ZLIB_COMPATIBLE_NAMES MINIZ_NO_STDIO)
