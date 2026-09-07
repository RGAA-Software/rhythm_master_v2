add_library(cyber_ui STATIC theme.cpp)
target_include_directories(cyber_ui PUBLIC include)
target_link_libraries(cyber_ui PRIVATE spike_imgui)
rhythm_project_target(cyber_ui)
