add_library(export_core STATIC render_export.cpp soundtrack.cpp)
target_include_directories(export_core PUBLIC include)
target_link_libraries(export_core PUBLIC media_av_writer project_io Rhythm::Render
    PRIVATE graph_runtime video_sources audio_analysis)
rhythm_project_target(export_core)
