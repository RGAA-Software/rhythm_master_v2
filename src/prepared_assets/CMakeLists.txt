add_library(prepared_assets STATIC prepare.cpp shader_assets.cpp loader.cpp video_assets.cpp soundtrack.cpp audio_arrangement_assets.cpp)
target_sources(prepared_assets PRIVATE text_assets.cpp)
target_link_libraries(prepared_assets PRIVATE text_layout)
target_include_directories(prepared_assets PUBLIC include)
target_link_libraries(prepared_assets PUBLIC model_assets media_types image_shader PRIVATE asset_store)
if(RHYTHM_BUILD_MEDIA)
    target_link_libraries(prepared_assets PRIVATE media_video)
    target_compile_definitions(prepared_assets PRIVATE RHYTHM_HAS_IMAGE_DECODER=1)
endif()
rhythm_project_target(prepared_assets)
if(BUILD_TESTING)
    if(RHYTHM_BUILD_MEDIA)
        add_executable(mixed_cache_tests tests/mixed_cache_tests.cpp)
        target_include_directories(mixed_cache_tests PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/third_party/sources/picosha2")
        target_link_libraries(mixed_cache_tests PRIVATE prepared_assets text_layout)
        rhythm_project_target(mixed_cache_tests)
        if(WIN32)
            add_test(NAME mixed_preparation_cache COMMAND mixed_cache_tests
                "${PROJECT_SOURCE_DIR}/third_party/assets/noto-cjk/NotoSansCJKsc-Regular.otf"
                "${PROJECT_SOURCE_DIR}/third_party/assets/khronos-box/Box.glb"
                "${PROJECT_SOURCE_DIR}/src/media/tests/fixtures/alpha.png"
                "${PROJECT_BINARY_DIR}/video-playback-fixtures/视频.mkv")
            set_tests_properties(mixed_preparation_cache PROPERTIES
                FIXTURES_REQUIRED video_playback_media TIMEOUT 60)
        endif()
    endif()
    add_executable(text_preparation_tests tests/text_preparation_tests.cpp)
    target_include_directories(text_preparation_tests PRIVATE "${PROJECT_SOURCE_DIR}/third_party/sources/picosha2")
    target_include_directories(text_preparation_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(text_preparation_tests PRIVATE text_layout)
    target_link_libraries(text_preparation_tests PRIVATE prepared_assets graph_runtime runtime_package)
    rhythm_project_target(text_preparation_tests)
    add_test(NAME text_preparation COMMAND text_preparation_tests
        "${PROJECT_SOURCE_DIR}/third_party/assets/noto-cjk/NotoSansCJKsc-Regular.otf")
endif()
