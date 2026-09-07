#include "rhythm/platform/host.h"

#include <SDL3/SDL.h>
#include <backends/imgui_impl_sdl3.h>
#include <imgui.h>

#include <atomic>
#include <cstdlib>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif
#include <map>
#include <stdexcept>

#include "bgfx_backend.h"
#include "ui_bridge.h"

namespace rhythm::platform {
namespace {
std::filesystem::path ExecutableDirectory() {
    const auto path = SDL_GetBasePath();
    if (!path) throw std::runtime_error(SDL_GetError());
    const std::string utf8(path);
    return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
}
std::atomic_flag host_in_use = ATOMIC_FLAG_INIT;
class HostLease final {
   public:
    HostLease() {
        if (host_in_use.test_and_set()) throw std::logic_error("platform.host_exists");
    }
    ~HostLease() { host_in_use.clear(); }
    HostLease(const HostLease&) = delete;
    HostLease& operator=(const HostLease&) = delete;
};
class SdlLifetime final {
   public:
    SdlLifetime() {
        if (!SDL_Init(SDL_INIT_VIDEO)) throw std::runtime_error(SDL_GetError());
    }
    ~SdlLifetime() { SDL_Quit(); }
    SdlLifetime(const SdlLifetime&) = delete;
    SdlLifetime& operator=(const SdlLifetime&) = delete;

   private:
    HostLease lease_{};
};
struct WindowDeleter {
    void operator()(SDL_Window* window) const { SDL_DestroyWindow(window); }
};
struct ContextDeleter {
    void operator()(ImGuiContext* context) const { ImGui::DestroyContext(context); }
};
struct SdlStringDeleter {
    void operator()(char* text) const { SDL_free(text); }
};
struct WindowState final {
    SdlLifetime sdl_{};
    // SDL returns exclusive window ownership; only this boundary stores its pointer.
    std::unique_ptr<SDL_Window, WindowDeleter> window_{};
    explicit WindowState(bool hidden) {
        auto flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
        if (hidden) flags |= SDL_WINDOW_HIDDEN;
        window_.reset(SDL_CreateWindow("Rhythm Master", 1280, 800, flags));
        if (!window_) throw std::runtime_error(SDL_GetError());
    }
};
class ImGuiPlatform final {
   public:
    explicit ImGuiPlatform(SDL_Window& window) {
        if (!ImGui_ImplSDL3_InitForOther(&window)) throw std::runtime_error("ui.sdl_init");
    }
    ~ImGuiPlatform() { ImGui_ImplSDL3_Shutdown(); }
    ImGuiPlatform(const ImGuiPlatform&) = delete;
    ImGuiPlatform& operator=(const ImGuiPlatform&) = delete;
};
}  // namespace

class Host::Impl final {
   public:
    explicit Impl(bool hidden) : window_(std::make_shared<WindowState>(hidden)) {
        context_.reset(ImGui::CreateContext());
        if (!context_) throw std::runtime_error("ui.context");
        auto& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_NavEnableKeyboard;
        io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
        io.IniFilename = nullptr;
        ImGui::StyleColorsDark();
        // Read the locally installed font; system fonts are not redistributed.
        if (std::filesystem::exists("C:/Windows/Fonts/msyh.ttc")) {
            if (!io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/msyh.ttc", 18, nullptr,
                                              io.Fonts->GetGlyphRangesChineseFull()))
                throw std::runtime_error("ui.font_load");
        } else {
            io.Fonts->AddFontDefault();
        }
        platform_ = std::make_unique<ImGuiPlatform>(*window_->window_);
    }
    std::shared_ptr<WindowState> window_{};
    std::unique_ptr<ImGuiContext, ContextDeleter> context_{};
    std::unique_ptr<ImGuiPlatform> platform_{};
    std::map<std::uint64_t, render::TextureHandle> textures_{};
    std::uint64_t next_texture_ = 2;
};

Host::Host(bool hidden) {
#ifdef _MSC_VER
    // Test failures must report to stderr rather than hang unattended runs in CRT dialogs.
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    impl_ = std::make_unique<Impl>(hidden);
}
Host::~Host() = default;
render::Renderer Host::CreateRenderer() {
    int width = 0;
    int height = 0;
    if (!SDL_GetWindowSizeInPixels(impl_->window_->window_.get(), &width, &height))
        throw std::runtime_error(SDL_GetError());
    const auto properties = SDL_GetWindowProperties(impl_->window_->window_.get());
    // Borrowed HWND is confined to this native-adapter call. Shared WindowState owns its lifetime.
    const auto native =
            SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
    if (!native) throw std::runtime_error("platform.no_native_window");
    return render::Renderer(render::detail::CreateBgfxBackend(
            reinterpret_cast<std::uintptr_t>(native),
            {static_cast<std::uint16_t>(width), static_cast<std::uint16_t>(height)},
            impl_->window_));
}
bool Host::Poll() {
    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL3_ProcessEvent(&event);
        if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
            return false;
    }
    if (SDL_GetWindowFlags(impl_->window_->window_.get()) & SDL_WINDOW_MINIMIZED) SDL_Delay(16);
    return true;
}
bool Host::IsSuspended() const {
    return (SDL_GetWindowFlags(impl_->window_->window_.get()) & SDL_WINDOW_MINIMIZED) != 0;
}
void Host::Resize(render::Extent logical_size) {
    if (!logical_size.width_ || !logical_size.height_ || logical_size.width_ > 8192 ||
        logical_size.height_ > 8192)
        throw std::invalid_argument("platform.window_extent");
    if (!SDL_SetWindowSize(impl_->window_->window_.get(), logical_size.width_,
                           logical_size.height_))
        throw std::runtime_error(SDL_GetError());
}
void Host::BeginUi() {
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}
render::DrawList Host::EndUi() {
    ImGui::Render();
    return TranslateUi(impl_->textures_);
}
render::Texture Host::CreateFontTexture(render::Renderer& renderer) {
    // Font atlas pixels are borrowed only for the synchronous upload, which copies them.
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    auto& fonts = *ImGui::GetIO().Fonts;
    fonts.GetTexDataAsRGBA32(&pixels, &width, &height);
    if (!pixels || width <= 0 || height <= 0 || width > 8192 || height > 8192)
        throw std::runtime_error("ui.font_atlas");
    auto texture = renderer.CreateTexture(
            {static_cast<std::uint16_t>(width), static_cast<std::uint16_t>(height)},
            std::span<const std::uint8_t>(pixels, static_cast<std::size_t>(width) * height * 4));
    impl_->textures_[1] = texture.Handle();
    fonts.SetTexID(1);
    return texture;
}
std::uint64_t Host::RegisterTexture(render::TextureHandle texture) {
    const auto id = impl_->next_texture_++;
    impl_->textures_.emplace(id, texture);
    return id;
}
void Host::ClearViewerTextures() {
    impl_->textures_.erase(impl_->textures_.upper_bound(1), impl_->textures_.end());
}
std::filesystem::path Host::DataDirectory() const {
    std::unique_ptr<char, SdlStringDeleter> path(SDL_GetPrefPath("RhythmMaster", "Studio"));
    if (!path) throw std::runtime_error(SDL_GetError());
    const std::string utf8(path.get());
    return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
}
std::filesystem::path Host::ResourceDirectory() const { return ExecutableDirectory(); }
}  // namespace rhythm::platform
