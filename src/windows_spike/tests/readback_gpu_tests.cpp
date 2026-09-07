#include <iostream>
#include <stdexcept>

#include "rhythm/platform/host.h"

namespace rhythm::validation {
void VerifyReadbackPixels(render::Renderer& renderer);
}
int main() {
    try {
        rhythm::platform::Host host(true);
        {
            auto renderer = host.CreateRenderer();
            rhythm::validation::VerifyReadbackPixels(renderer);
            auto target = renderer.CreateTexture({16, 16});
            renderer.BeginFrame();
            auto pending = renderer.RequestReadback(target.Handle());
            renderer.EndFrame();
            renderer.Invalidate();
            bool rejected = false;
            try {
                pending.Poll();
            } catch (const std::exception&) {
                rejected = true;
            }
            if (!rejected) throw std::runtime_error("lost device delivered a stale readback");
            // Leave an issued read pending: shutdown must precede pixel release.
        }
        auto replacement = host.CreateRenderer();
        rhythm::validation::VerifyReadbackPixels(replacement);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
