#include "host.h"

#include <SDL3/SDL.h>
#include <android/native_window.h>

#include <stdexcept>

#include "bgfx_backend.h"
#include "commands.h"

namespace rhythm::platform {
namespace {
class SdlLifetime final {
   public:
    SdlLifetime() {
        SDL_SetHint(SDL_HINT_ANDROID_BLOCK_ON_PAUSE, "0");
        if (!SDL_Init(SDL_INIT_VIDEO)) throw std::runtime_error(SDL_GetError());
    }
    ~SdlLifetime() { SDL_Quit(); }
};
struct WindowDeleter {
    void operator()(SDL_Window* window) const { SDL_DestroyWindow(window); }
};
struct StreamDeleter {
    void operator()(SDL_IOStream* stream) const { SDL_CloseIO(stream); }
};
class Properties final {
   public:
    Properties() : id_(SDL_CreateProperties()) {
        if (!id_) throw std::runtime_error(SDL_GetError());
    }
    ~Properties() { SDL_DestroyProperties(id_); }
    SDL_PropertiesID Get() const { return id_; }

   private:
    SDL_PropertiesID id_ = 0;
};
struct WindowState {
    SdlLifetime sdl_{};
    // Exclusive SDL ownership remains in this adapter.
    std::unique_ptr<SDL_Window, WindowDeleter> window_{};
    WindowState() {
        Properties properties;
        SDL_SetStringProperty(properties.Get(), SDL_PROP_WINDOW_CREATE_TITLE_STRING,
                              "Rhythm Master Player");
        SDL_SetBooleanProperty(properties.Get(),
                               SDL_PROP_WINDOW_CREATE_EXTERNAL_GRAPHICS_CONTEXT_BOOLEAN, true);
        SDL_SetBooleanProperty(properties.Get(), SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, true);
        window_.reset(SDL_CreateWindowWithProperties(properties.Get()));
        if (!window_) throw std::runtime_error(SDL_GetError());
    }
};
struct SurfaceState {
    std::shared_ptr<WindowState> window_{};
    // Owns one acquired ANativeWindow reference, independently of SDL surface events.
    std::shared_ptr<void> surface_{};
};
}  // namespace
class Host::Impl final {
   public:
    std::shared_ptr<WindowState> window_ = std::make_shared<WindowState>();
    bool suspended_ = false;
};
Host::Host() : impl_(std::make_unique<Impl>()) {}
Host::~Host() = default;
bool Host::Poll() {
    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_TERMINATING) return false;
        if (event.type == SDL_EVENT_WILL_ENTER_BACKGROUND) impl_->suspended_ = true;
        if (event.type == SDL_EVENT_DID_ENTER_FOREGROUND) impl_->suspended_ = false;
    }
    return true;
}
bool Host::Suspended() const {
    return impl_->suspended_ || !android_host::CurrentSurface().window_;
}
std::uint64_t Host::SurfaceGeneration() const { return android_host::CurrentSurface().generation_; }
render::Extent Host::Size() const {
    int width = 0;
    int height = 0;
    if (!SDL_GetWindowSizeInPixels(impl_->window_->window_.get(), &width, &height) || width < 1 ||
        height < 1 || width > 8192 || height > 8192)
        return {};
    return {static_cast<std::uint16_t>(width), static_cast<std::uint16_t>(height)};
}
render::Renderer Host::CreateRenderer() {
    const auto surface = android_host::CurrentSurface();
    if (!surface.window_) throw std::runtime_error("platform.no_native_surface");
    auto owner = std::make_shared<SurfaceState>();
    owner->window_ = impl_->window_;
    owner->surface_ = surface.owner_;
    return render::Renderer(render::detail::CreateBgfxBackend(surface.window_, Size(), owner));
}
std::string Host::ReadAsset(const std::string& name) const {
    std::unique_ptr<SDL_IOStream, StreamDeleter> stream(SDL_IOFromFile(name.c_str(), "rb"));
    if (!stream) throw std::runtime_error("player.asset_open");
    const auto size = SDL_GetIOSize(stream.get());
    if (size < 1 || size > 16 * 1024 * 1024) throw std::runtime_error("player.asset_size");
    std::string bytes(static_cast<std::size_t>(size), '\0');
    if (SDL_ReadIO(stream.get(), bytes.data(), bytes.size()) != bytes.size())
        throw std::runtime_error("player.asset_read");
    return bytes;
}
std::filesystem::path Host::DataDirectory() const {
    const auto path = SDL_GetAndroidInternalStoragePath();
    if (!path) throw std::runtime_error("platform.storage_unavailable");
    return path;
}
std::filesystem::path Host::CacheDirectory() const {
    const auto path = SDL_GetAndroidCachePath();
    if (!path) throw std::runtime_error("platform.cache_unavailable");
    return path;
}
}  // namespace rhythm::platform
