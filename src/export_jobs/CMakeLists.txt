add_library(export_job_files STATIC job_files.cpp)
target_include_directories(export_job_files PRIVATE .)
target_link_libraries(export_job_files PUBLIC export_types PRIVATE atomic_storage nlohmann_json::nlohmann_json)
rhythm_project_target(export_job_files)
add_library(export_jobs STATIC jobs.cpp export_process.cpp workspace.cpp)
target_include_directories(export_jobs PUBLIC include)
target_link_libraries(export_jobs PUBLIC export_types editor_application
    PRIVATE export_job_files project_io atomic_storage foundation_async SDL3::SDL3-static)
rhythm_project_target(export_jobs)
add_library(export_job_host STATIC job_host.cpp)
target_link_libraries(export_job_host PUBLIC export_jobs PRIVATE export_job_files export_core platform_sdl)
rhythm_project_target(export_job_host)
if(BUILD_TESTING)
    add_executable(export_job_metadata_tests tests/job_metadata_tests.cpp)
    target_include_directories(export_job_metadata_tests PRIVATE .)
    target_link_libraries(export_job_metadata_tests PRIVATE export_job_files atomic_storage)
    rhythm_project_target(export_job_metadata_tests)
    add_test(NAME export_job_metadata COMMAND export_job_metadata_tests
        "${PROJECT_BINARY_DIR}/export-metadata")
    add_executable(export_job_tests tests/job_tests.cpp)
    target_link_libraries(export_job_tests PRIVATE export_jobs platform_sdl media_video project_io)
    rhythm_project_target(export_job_tests)
    add_dependencies(export_job_tests rhythm_master)
    add_test(NAME export_jobs COMMAND export_job_tests "$<TARGET_FILE:rhythm_master>"
        "${PROJECT_BINARY_DIR}/content/templates/resonance_live"
        "${PROJECT_BINARY_DIR}/music-fixtures/resonance_demo.wav"
        "${PROJECT_BINARY_DIR}/export-jobs")
    set_tests_properties(export_jobs PROPERTIES FIXTURES_REQUIRED music_pcm TIMEOUT 120 RUN_SERIAL TRUE)
endif()
