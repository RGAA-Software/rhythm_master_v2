#include <cstdlib>
#include <iostream>
#include <stdexcept>

#include "rhythm/render/layout.h"
#include "rhythm/render/renderer.h"

namespace {
void Check(bool condition) {
    if (!condition) throw std::runtime_error("render contract assertion failed");
}
template <typename Function>
void Reject(Function function) {
    bool rejected = false;
    try {
        function();
    } catch (const std::exception&) {
        rejected = true;
    }
    Check(rejected);
}
}  // namespace
int main() {
    using namespace rhythm::render;
    try {
        const auto portrait = AspectFit({720, 1280}, {0, 0, 256, 144});
        Check(portrait.x_ == 87.5f && portrait.y_ == 0 && portrait.width_ == 81 &&
              portrait.height_ == 144);
        const auto square = AspectFit({1024, 1024}, {10, 20, 256, 144});
        Check(square.x_ == 66 && square.y_ == 20 && square.width_ == 144 && square.height_ == 144);
        Reject([] { AspectFit({}, {0, 0, 100, 100}); });
        auto renderer = Renderer::CreateNull();
        auto second = Renderer::CreateNull();
        Reject([&] { renderer.CreateTexture({0, 10}); });
        Reject([&] { renderer.EndFrame(); });
        TextureHandle stale;
        {
            auto texture = renderer.CreateTexture({16, 16});
            stale = texture.Handle();
            Check(renderer.IsValid(stale));
            Check(!second.IsValid(stale));
            auto moved = std::move(texture);
            Check(texture.Handle() == TextureHandle{});
            Check(renderer.Stats().texture_bytes_ == 1024);
        }
        auto replacement = renderer.CreateTexture({16, 16});
        Check(!renderer.IsValid(stale));
        Check(replacement.Handle() != stale);
        DrawList draw;
        draw.width_ = 16;
        draw.height_ = 16;
        draw.vertices_ = {{0, 0}, {16, 0}, {0, 16}};
        draw.indices_ = {0, 1, 2};
        draw.commands_ = {{replacement.Handle(), 0, 3, {0, 0, 16, 16}}};
        Reject([&] { renderer.Submit({}, draw); });
        renderer.BeginFrame();
        Reject([&] { renderer.BeginFrame(); });
        Reject([&] { renderer.Submit(replacement.Handle(), draw); });
        renderer.Submit({}, draw);
        draw.commands_[0].blend_ = static_cast<BlendMode>(255);
        Reject([&] { renderer.Submit({}, draw); });
        draw.commands_[0].blend_ = BlendMode::kSourceOver;
        draw.commands_[0].color_adjustment_ = ColorAdjustment{9};
        Reject([&] { renderer.Submit({}, draw); });
        draw.commands_[0].color_adjustment_.reset();
        draw.commands_[0].texture_filter_ = TextureFilter{TextureFilterKind::kGaussian, -1, 0};
        Reject([&] { renderer.Submit({}, draw); });
        draw.commands_[0].texture_filter_ = TextureFilter{};
        draw.commands_[0].color_adjustment_ = ColorAdjustment{};
        Reject([&] { renderer.Submit({}, draw); });
        draw.commands_[0].color_adjustment_.reset();
        draw.commands_[0].texture_filter_.reset();
        draw.commands_[0].texture_noise_ = TextureNoise{};
        draw.commands_[0].texture_noise_->scale_ = 0;
        Reject([&] { renderer.Submit({}, draw); });
        draw.commands_[0].texture_noise_->scale_ = 4;
        draw.commands_[0].texture_noise_->color_a_[0] = -1;
        Reject([&] { renderer.Submit({}, draw); });
        draw.commands_[0].texture_noise_.reset();
        draw.commands_[0].texture_mapping_ = TextureMapping{};
        draw.commands_[0].texture_mapping_->scale_ = 0;
        Reject([&] { renderer.Submit({}, draw); });
        draw.commands_[0].texture_mapping_->scale_ = 1;
        draw.commands_[0].texture_mapping_->sectors_ = 2.5f;
        Reject([&] { renderer.Submit({}, draw); });
        draw.commands_[0].texture_mapping_.reset();
        draw.commands_[0].texture_contours_ = TextureContours{};
        draw.commands_[0].texture_contours_->width_ = 0;
        Reject([&] { renderer.Submit({}, draw); });
        draw.commands_[0].texture_contours_->width_ = 0.1f;
        draw.commands_[0].texture_mapping_ = TextureMapping{};
        Reject([&] { renderer.Submit({}, draw); });
        draw.commands_[0].texture_mapping_.reset();
        draw.commands_[0].texture_contours_.reset();
        draw.indices_[0] = 999;
        Reject([&] { renderer.Submit({}, draw); });
        renderer.EndFrame();
        Check(renderer.Stats().draws_ == 1 && renderer.Stats().frame_ == 1);
        replacement = {};
        Check(renderer.Stats().live_textures_ == 0);
        auto moved_renderer = std::move(renderer);
        Check(!renderer.IsValid(stale));
        Reject([&] { renderer.BeginFrame(); });
        Reject([&] { renderer.CreateTexture({16, 16}); });
        Reject([&] { static_cast<void>(renderer.Stats()); });
        moved_renderer.BeginFrame();
        moved_renderer.EndFrame();
        auto lost_texture = moved_renderer.CreateTexture({16, 16});
        const auto lost_handle = lost_texture.Handle();
        moved_renderer.Invalidate();
        Check(!moved_renderer.IsValid(lost_handle));
        Reject([&] { moved_renderer.BeginFrame(); });
        Reject([&] { moved_renderer.CreateTexture({16, 16}); });
        lost_texture = {};
        Check(moved_renderer.Stats().live_textures_ == 0);
        moved_renderer = Renderer::CreateNull();
        Check(!moved_renderer.IsValid(lost_handle));
        auto restored = moved_renderer.CreateTexture({16, 16});
        Check(moved_renderer.IsValid(restored.Handle()));
        std::cout << "render contracts passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
