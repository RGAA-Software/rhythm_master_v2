#include <cstdlib>
#include <iostream>
#include <limits>
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
        Check(!renderer.SupportsReadback());
        Reject([&] { renderer.RequestReadback({}); });
        Readback absent;
        Reject([&] { absent.Poll(); });
        auto second = Renderer::CreateNull();
        {
            auto original = renderer.CreateTexture({32, 32});
            const auto handle = original.Handle();
            auto retained = renderer.RetainTexture(handle);
            Check(renderer.Stats().texture_bytes_ == 32 * 32 * 4 &&
                  renderer.Stats().live_textures_ == 1);
            Reject([&] { second.RetainTexture(handle); });
            Reject([&] { renderer.RetainTexture({}); });
            original = {};
            Check(renderer.IsValid(handle));
            auto last = renderer.RetainTexture(retained.Handle());
            retained = {};
            Check(renderer.IsValid(last.Handle()) &&
                  renderer.Stats().texture_bytes_ == 32 * 32 * 4);
            last = {};
            Check(!renderer.IsValid(handle) && renderer.Stats().texture_bytes_ == 0);
            Reject([&] { renderer.RetainTexture(handle); });
        }
        {
            const std::array<std::uint8_t, 4> pixel{120, 40, 200, 64};
            auto image = renderer.CreateTexture({1, 1}, pixel);
            auto target = renderer.CreateTexture({1, 1});
            auto foreign = second.CreateTexture({1, 1}, pixel);
            const auto bytes = renderer.Stats().texture_bytes_;
            Reject([&] { renderer.UpdateTexture(image.Handle(), pixel); });
            renderer.BeginFrame();
            renderer.UpdateTexture(image.Handle(), pixel);
            Check(renderer.Stats().texture_bytes_ == bytes);
            Reject([&] { renderer.UpdateTexture(target.Handle(), pixel); });
            Reject([&] { renderer.UpdateTexture(foreign.Handle(), pixel); });
            Reject([&] { renderer.UpdateTexture(image.Handle(), {}); });
            DrawList empty;
            empty.width_ = empty.height_ = 1;
            renderer.Submit({}, empty);
            renderer.UpdateTexture(image.Handle(), pixel);
            empty.vertices_ = {{0, 0}, {1, 0}, {0, 1}};
            empty.indices_ = {0, 1, 2};
            empty.commands_ = {{image.Handle(), 0, 3, {0, 0, 1, 1}}};
            renderer.Submit({}, empty);
            Reject([&] { renderer.UpdateTexture(image.Handle(), pixel); });
            renderer.EndFrame();
            const auto stale_image = image.Handle();
            image = {};
            renderer.BeginFrame();
            Reject([&] { renderer.UpdateTexture(stale_image, pixel); });
            renderer.EndFrame();
        }
        {
            auto history = renderer.CreateTexture({16, 16}, {}, TexturePrecision::kFloat16);
            Check(renderer.Stats().texture_bytes_ == 16 * 16 * 8);
            Check(renderer.Precision(history.Handle()) == TexturePrecision::kFloat16);
            Reject([&] { second.Precision(history.Handle()); });
            const std::array<std::uint8_t, 4> pixel{255, 255, 255, 255};
            Reject([&] { renderer.CreateTexture({1, 1}, pixel, TexturePrecision::kFloat16); });
            Reject([&] { renderer.CreateTexture({1, 1}, {}, static_cast<TexturePrecision>(255)); });
        }
        {
            Check(renderer.SupportsSampleableDepth());
            auto color = renderer.CreateTexture({16, 16});
            auto depth = renderer.CreateDepthTexture({16, 16});
            auto wrong_size = renderer.CreateDepthTexture({8, 16});
            auto foreign = second.CreateDepthTexture({16, 16});
            Reject([&] { renderer.Precision(depth.Handle()); });
            renderer.BeginFrame();
            Reject([&] { renderer.SubmitSceneDepth(color.Handle(), foreign.Handle(), {}); });
            Reject([&] { renderer.SubmitSceneDepth(color.Handle(), wrong_size.Handle(), {}); });
            Reject([&] { renderer.SubmitSceneDepth(depth.Handle(), color.Handle(), {}); });
            const auto bytes = renderer.Stats().texture_bytes_;
            renderer.SubmitSceneDepth(color.Handle(), depth.Handle(), {});
            SceneDrawList lighting;
            lighting.positional_lights_ = {PositionalLight{}};
            renderer.SubmitSceneDepth(color.Handle(), depth.Handle(), lighting);
            lighting.positional_lights_[0].range_ = 0;
            Reject([&] { renderer.SubmitSceneDepth(color.Handle(), depth.Handle(), lighting); });
            lighting.positional_lights_[0] = {};
            lighting.positional_lights_[0].position_[0] = std::numeric_limits<float>::quiet_NaN();
            Reject([&] { renderer.SubmitSceneDepth(color.Handle(), depth.Handle(), lighting); });
            lighting.positional_lights_[0] = {};
            lighting.positional_lights_.resize(4);
            lighting.lights_.resize(1);
            Reject([&] { renderer.SubmitSceneDepth(color.Handle(), depth.Handle(), lighting); });
            Check(renderer.Stats().texture_bytes_ == bytes);
            renderer.SubmitScene(color.Handle(), {});
            Check(renderer.Stats().texture_bytes_ == bytes + 16 * 16 * 4);
            renderer.SubmitSceneDepth(color.Handle(), depth.Handle(), {});
            Check(renderer.Stats().texture_bytes_ == bytes);
            DrawList draw;
            draw.width_ = draw.height_ = 16;
            draw.vertices_ = {{0, 0}, {16, 0}, {0, 16}};
            draw.indices_ = {0, 1, 2};
            draw.commands_ = {{depth.Handle(), 0, 3, {0, 0, 16, 16}}};
            Reject([&] { renderer.Submit(color.Handle(), draw); });
            draw.commands_[0].depth_linearization_ = DepthLinearization{};
            renderer.Submit(color.Handle(), draw);
            Reject([&] { renderer.Submit(depth.Handle(), draw); });
            draw.commands_[0].depth_linearization_->far_ = 0;
            Reject([&] { renderer.Submit(color.Handle(), draw); });
            const auto stale = depth.Handle();
            depth = {};
            Check(!renderer.IsValid(stale));
            Reject([&] { renderer.SubmitSceneDepth(color.Handle(), stale, {}); });
            renderer.EndFrame();
        }
        const auto initial_frame = renderer.Stats().frame_;
        Check(renderer.Stats().texture_bytes_ == 0);
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
        draw.commands_[0].texture_noise_->offset_x_ = 4097;
        Reject([&] { renderer.Submit({}, draw); });
        draw.commands_[0].texture_noise_->offset_x_ = 0;
        draw.commands_[0].texture_noise_->offset_y_ = -4097;
        Reject([&] { renderer.Submit({}, draw); });
        draw.commands_[0].texture_noise_->offset_y_ = 0;
        draw.commands_[0].texture_noise_->offset_x_ = std::numeric_limits<float>::quiet_NaN();
        Reject([&] { renderer.Submit({}, draw); });
        draw.commands_[0].texture_noise_->offset_x_ = 0;
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
        {
            auto map = renderer.CreateTexture({8, 8});
            auto foreign = second.CreateTexture({8, 8});
            auto& effect = draw.commands_[0].texture_displace_;
            effect = TextureDisplace{map.Handle()};
            renderer.Submit({}, draw);
            Reject([&] { renderer.Submit(map.Handle(), draw); });
            for (const auto invalid : {TextureHandle{}, stale, foreign.Handle()}) {
                effect->map_ = invalid;
                Reject([&] { renderer.Submit({}, draw); });
            }
            effect->map_ = map.Handle();
            for (const float invalid : {-2.0f, 2.0f, std::numeric_limits<float>::quiet_NaN()}) {
                effect->strength_ = invalid;
                Reject([&] { renderer.Submit({}, draw); });
            }
            effect->strength_ = 0;
            effect->radius_ = 0;
            Reject([&] { renderer.Submit({}, draw); });
            effect->radius_ = 2;
            effect->kind_ = static_cast<TextureDisplaceKind>(255);
            Reject([&] { renderer.Submit({}, draw); });
            effect->kind_ = TextureDisplaceKind::kGradient;
            draw.commands_[0].texture_filter_ = TextureFilter{};
            Reject([&] { renderer.Submit({}, draw); });
            draw.commands_[0].texture_filter_.reset();
            effect.reset();
            auto& trail = draw.commands_[0].texture_trail_;
            trail = TextureTrail{map.Handle(), 0.5f};
            renderer.Submit({}, draw);
            Reject([&] { renderer.Submit(map.Handle(), draw); });
            trail->retention_ = std::numeric_limits<float>::quiet_NaN();
            Reject([&] { renderer.Submit({}, draw); });
            trail->retention_ = 0.5f;
            trail->history_ = foreign.Handle();
            Reject([&] { renderer.Submit({}, draw); });
            trail.reset();
        }
        draw.indices_[0] = 999;
        Reject([&] { renderer.Submit({}, draw); });
        renderer.EndFrame();
        Check(renderer.Stats().draws_ == 3 && renderer.Stats().frame_ == initial_frame + 1);
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
