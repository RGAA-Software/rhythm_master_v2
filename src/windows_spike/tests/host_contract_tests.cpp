#include <iostream>
#include <stdexcept>

#include "rhythm/platform/host.h"

namespace {
template <typename Function>
void Reject(Function action, const std::string& expected) {
    try {
        action();
    } catch (const std::logic_error& error) {
        if (error.what() == expected) return;
        throw;
    }
    throw std::runtime_error("host.expected_rejection");
}
}  // namespace
int main() {
    using namespace rhythm;
    try {
        render::Texture retained;
        for (int cycle = 0; cycle < 3; ++cycle) {
            {
                platform::Host host(true);
                Reject([] { platform::Host duplicate(true); }, "platform.host_exists");
                auto renderer = host.CreateRenderer();
                Reject([&] { host.CreateRenderer(); }, "render.device_exists");
                auto font = host.CreateFontTexture(renderer);
                retained = renderer.CreateTexture({16, 16});
                for (int frame = 0; frame < 6; ++frame) {
                    const render::Extent extent =
                            frame % 2 ? render::Extent{960, 540} : render::Extent{640, 360};
                    host.Resize(extent);
                    if (!host.Poll()) throw std::runtime_error("host.closed");
                    host.BeginUi();
                    const auto draw = host.EndUi();
                    renderer.BeginFrame();
                    renderer.Submit({}, draw, 0x123456ff);
                    renderer.EndFrame();
                    if (!renderer.IsValid(retained.Handle()))
                        throw std::runtime_error("host.resize_lost_resource");
                }
                const auto lost_handle = retained.Handle();
                renderer.Invalidate();
                if (renderer.IsValid(lost_handle)) throw std::runtime_error("host.loss_handle");
                Reject([&] { renderer.BeginFrame(); }, "render.device_lost");
                retained = {};
                font = {};
                renderer = render::Renderer::CreateNull();
                renderer = host.CreateRenderer();
                if (renderer.IsValid(lost_handle)) throw std::runtime_error("host.reused_device");
                font = host.CreateFontTexture(renderer);
                retained = renderer.CreateTexture({16, 16});
                host.BeginUi();
                const auto restored_draw = host.EndUi();
                renderer.BeginFrame();
                renderer.Submit({}, restored_draw, 0x345678ff);
                renderer.EndFrame();
            }
            // Resource ownership intentionally outlives Host and Renderer. SDL
            // and the native surface must stay alive until the last GPU owner.
            Reject([] { platform::Host duplicate(true); }, "platform.host_exists");
            retained = {};
        }
        std::cout << "host contracts passed: 3 lifetimes, 18 resizes, 3 injected device rebuilds, "
                     "retained GPU resource\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
