if(ANDROID)
    set(text_triplet arm64-android)
elseif(WIN32)
    set(text_triplet x64-windows)
else()
    # Other hosts supply their own prefix; never select Windows object code.
    find_package(Freetype REQUIRED)
    find_package(utf8cpp CONFIG REQUIRED)
endif()
if(ANDROID OR WIN32)
    set(text_prefix "C:/source/vcpkg/installed/${text_triplet}")
    find_path(TEXT_FREETYPE_INCLUDE ft2build.h PATHS "${text_prefix}/include"
        NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH REQUIRED)
    # UTF8-CPP is header-only: use the target installation first, then the existing
    # host headers. No host object code is linked into the Android artifact.
    find_path(TEXT_UTF8_INCLUDE utf8.h PATHS "${text_prefix}/include/utf8cpp"
        "C:/source/vcpkg/installed/x64-windows/include/utf8cpp"
        NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH REQUIRED)
    if(ANDROID)
        add_library(text_freetype STATIC IMPORTED)
        set_target_properties(text_freetype PROPERTIES IMPORTED_LOCATION "${text_prefix}/lib/libfreetype.a")
        foreach(dependency png16 bz2 brotlidec brotlicommon)
            find_library(TEXT_${dependency}_LIBRARY NAMES ${dependency} PATHS "${text_prefix}/lib"
                NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH REQUIRED)
            set_property(TARGET text_freetype APPEND PROPERTY INTERFACE_LINK_LIBRARIES "${TEXT_${dependency}_LIBRARY}")
        endforeach()
        set_property(TARGET text_freetype APPEND PROPERTY INTERFACE_LINK_LIBRARIES z)
    else()
        add_library(text_freetype SHARED IMPORTED)
        set_target_properties(text_freetype PROPERTIES
            IMPORTED_IMPLIB "${text_prefix}/lib/freetype.lib"
            IMPORTED_LOCATION "${text_prefix}/bin/freetype.dll")
    endif()
endif()
add_library(text_layout STATIC rasterizer.cpp)
add_library(unicode_headers INTERFACE)
set_target_properties(text_layout PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(text_layout PUBLIC include)
if(ANDROID OR WIN32)
    target_include_directories(unicode_headers SYSTEM INTERFACE "${TEXT_UTF8_INCLUDE}")
    target_include_directories(text_layout SYSTEM PRIVATE "${TEXT_FREETYPE_INCLUDE}" "${TEXT_UTF8_INCLUDE}")
    target_link_libraries(text_layout PRIVATE text_freetype)
else()
    target_link_libraries(unicode_headers INTERFACE utf8cpp)
    target_link_libraries(text_layout PRIVATE Freetype::Freetype utf8cpp)
endif()
rhythm_project_target(text_layout)
if(BUILD_TESTING)
    add_executable(text_layout_tests tests/rasterizer_tests.cpp)
    target_link_libraries(text_layout_tests PRIVATE text_layout)
    rhythm_project_target(text_layout_tests)
    add_test(NAME text_layout_contracts COMMAND text_layout_tests
        "${PROJECT_SOURCE_DIR}/third_party/assets/noto-cjk/NotoSansCJKsc-Regular.otf")
endif()
