add_library(qr_reader STATIC reader.cpp async_reader.cpp)
target_include_directories(qr_reader PUBLIC include)
target_link_libraries(qr_reader PRIVATE ZXing::ZXing foundation_async)
rhythm_project_target(qr_reader)
