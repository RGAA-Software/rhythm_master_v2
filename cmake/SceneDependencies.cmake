# Header-only glTF parser supplied by vcpkg. Import remains an isolated probe.
if(ANDROID)
    set(rhythm_scene_triplet arm64-android)
else()
    set(rhythm_scene_triplet x64-windows)
endif()
find_path(RHYTHM_CGLTF_INCLUDE cgltf.h
    PATHS "C:/source/vcpkg/installed/${rhythm_scene_triplet}/include"
          "${PROJECT_SOURCE_DIR}/out/vcpkg-scene/${rhythm_scene_triplet}/include"
    NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH REQUIRED)
