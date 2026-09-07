add_library(export_types INTERFACE)
target_include_directories(export_types INTERFACE include "${PROJECT_SOURCE_DIR}/src/media/include")
add_library(export_core STATIC render_export.cpp soundtrack.cpp encoding_queue.cpp)
target_include_directories(export_core PUBLIC include)
target_link_libraries(export_core PUBLIC export_types media_av_writer project_io Rhythm::Render
    PRIVATE graph_runtime video_sources audio_analysis)
rhythm_project_target(export_core)
