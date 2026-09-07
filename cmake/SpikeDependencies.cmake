# Provisional Windows experiment, separate from the dependency-free shared core.
set(rhythm_deps "${PROJECT_SOURCE_DIR}/third_party/sources")
foreach(dependency sdl imgui node_editor bgfx bx bimg)
    if(NOT IS_DIRECTORY "${rhythm_deps}/${dependency}")
        message(FATAL_ERROR "Run tools/prepare-dependencies.ps1: missing ${dependency}")
    endif()
endforeach()
set(RHYTHM_SPIKE_SDK "${PROJECT_SOURCE_DIR}/../vcpkg/installed/x64-windows" CACHE PATH
    "Existing read-only SDK for the Windows experiment; not a production dependency lock")
list(PREPEND CMAKE_PREFIX_PATH "${RHYTHM_SPIKE_SDK}")
find_package(freetype CONFIG REQUIRED)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_RENDER OFF CACHE BOOL "" FORCE)
set(SDL_GPU OFF CACHE BOOL "" FORCE)
add_subdirectory("${rhythm_deps}/sdl" "${PROJECT_BINARY_DIR}/deps/sdl" EXCLUDE_FROM_ALL)
add_library(spike_imgui STATIC
    "${rhythm_deps}/imgui/imgui.cpp" "${rhythm_deps}/imgui/imgui_draw.cpp"
    "${rhythm_deps}/imgui/imgui_widgets.cpp" "${rhythm_deps}/imgui/imgui_tables.cpp"
    "${rhythm_deps}/imgui/backends/imgui_impl_sdl3.cpp"
    "${rhythm_deps}/imgui/misc/freetype/imgui_freetype.cpp")
target_include_directories(spike_imgui SYSTEM PUBLIC "${rhythm_deps}/imgui")
target_compile_definitions(spike_imgui PUBLIC IMGUI_ENABLE_FREETYPE IMGUI_DEFINE_MATH_OPERATORS)
target_link_libraries(spike_imgui PRIVATE SDL3::SDL3-static freetype)
add_library(spike_node_editor STATIC
    "${rhythm_deps}/node_editor/imgui_node_editor.cpp"
    "${rhythm_deps}/node_editor/imgui_node_editor_api.cpp"
    "${rhythm_deps}/node_editor/imgui_canvas.cpp"
    "${rhythm_deps}/node_editor/crude_json.cpp")
target_include_directories(spike_node_editor SYSTEM PUBLIC "${rhythm_deps}/node_editor")
target_link_libraries(spike_node_editor PRIVATE spike_imgui)
add_library(spike_bx STATIC "${rhythm_deps}/bx/src/amalgamated.cpp")
target_include_directories(spike_bx SYSTEM PUBLIC "${rhythm_deps}/bx/include")
target_include_directories(spike_bx PRIVATE "${rhythm_deps}/bx/3rdparty" "${rhythm_deps}/bx/include/compat/msvc")
add_library(spike_bimg STATIC "${rhythm_deps}/bimg/src/image.cpp")
target_include_directories(spike_bimg SYSTEM PUBLIC "${rhythm_deps}/bimg/include")
target_compile_definitions(spike_bimg PRIVATE BIMG_CONFIG_DECODE_ASTC=0)
target_link_libraries(spike_bimg PRIVATE spike_bx)
add_library(spike_bgfx STATIC "${rhythm_deps}/bgfx/src/amalgamated.cpp")
target_include_directories(spike_bgfx SYSTEM PUBLIC "${rhythm_deps}/bgfx/include")
target_include_directories(spike_bgfx PRIVATE "${rhythm_deps}/bgfx/3rdparty"
    "${rhythm_deps}/bgfx/3rdparty/khronos"
    "${rhythm_deps}/bgfx/3rdparty/directx-headers/include/directx")
target_compile_definitions(spike_bgfx PRIVATE
    BGFX_CONFIG_RENDERER_DIRECT3D11=1 BGFX_CONFIG_RENDERER_DIRECT3D12=0
    BGFX_CONFIG_RENDERER_OPENGL=0 BGFX_CONFIG_RENDERER_OPENGLES=0
    BGFX_CONFIG_RENDERER_VULKAN=0 BGFX_CONFIG_RENDERER_WEBGPU=0
    BGFX_CONFIG_MULTITHREADED=0 BGFX_CONFIG_VIDEO=0 WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(spike_bgfx PRIVATE spike_bx spike_bimg d3d11 dxgi dxguid dcomp psapi gdi32 version)
foreach(target spike_bx spike_bimg spike_bgfx)
    target_compile_features(${target} PRIVATE cxx_std_20)
    target_compile_definitions(${target} PRIVATE BX_CONFIG_DEBUG=0)
    target_compile_options(${target} PRIVATE /Zc:__cplusplus /Zc:preprocessor /bigobj)
endforeach()
target_compile_options(spike_bx PRIVATE /EHs-c-)
