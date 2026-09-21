#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

#include "gpu_execution_probe.h"
#include "rhythm/render/renderer.h"

namespace rhythm::validation {
namespace {
render::Matrix4 Multiply(const render::Matrix4& first, const render::Matrix4& second) {
    render::Matrix4 result{};
    for (std::size_t column = 0; column < 4; ++column)
        for (std::size_t row = 0; row < 4; ++row)
            for (std::size_t inner = 0; inner < 4; ++inner)
                result[column * 4 + row] += first[inner * 4 + row] * second[column * 4 + inner];
    return result;
}
render::Matrix4 PointShadowMatrix(std::size_t face) {
    constexpr std::array<std::array<float, 3>, 6> kForward{
            {{{1, 0, 0}}, {{-1, 0, 0}}, {{0, -1, 0}}, {{0, 1, 0}}, {{0, 0, 1}}, {{0, 0, -1}}}};
    constexpr std::array<std::array<float, 3>, 6> kRight{
            {{{0, 0, -1}}, {{0, 0, 1}}, {{1, 0, 0}}, {{1, 0, 0}}, {{1, 0, 0}}, {{-1, 0, 0}}}};
    constexpr std::array<std::array<float, 3>, 6> kUp{
            {{{0, -1, 0}}, {{0, -1, 0}}, {{0, 0, -1}}, {{0, 0, 1}}, {{0, -1, 0}}, {{0, -1, 0}}}};
    const auto& forward = kForward[face];
    const auto& right = kRight[face];
    const auto& up = kUp[face];
    const render::Matrix4 view{right[0], up[0], -forward[0], 0, right[1], up[1], -forward[1], 0,
                               right[2], up[2], -forward[2], 0, 0,        0,     0,           1};
    constexpr float kNear = 0.1f;
    constexpr float kFar = 10.0f;
    constexpr float kDepth = kFar - kNear;
    const render::Matrix4 projection{1,
                                     0,
                                     0,
                                     0,
                                     0,
                                     1,
                                     0,
                                     0,
                                     0,
                                     0,
                                     -(kFar + kNear) / kDepth,
                                     -1,
                                     0,
                                     0,
                                     -2 * kNear * kFar / kDepth,
                                     0};
    return Multiply(projection, view);
}
}  // namespace
void VerifySceneShadows(render::Renderer& renderer) {
    using namespace render;
    const std::array<MeshVertex, 4> vertices{
            {{-0.95f, -0.95f}, {0.95f, -0.95f}, {0.95f, 0.95f}, {-0.95f, 0.95f}}};
    const std::array<std::uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
    auto mesh = renderer.CreateMesh(vertices, indices);
    auto shadow_color = renderer.CreateTexture({256, 256});
    auto shadow_depth = renderer.CreateDepthTexture({256, 256});
    auto cascade_color = renderer.CreateTexture({256, 256});
    auto cascade_depth = renderer.CreateDepthTexture({256, 256});
    auto output = renderer.CreateTexture({128, 128});
    SceneDrawList occluders;
    occluders.projection_[10] = -1;
    MeshDraw caster;
    caster.mesh_ = mesh.Handle();
    caster.double_sided_ = true;
    caster.model_[0] = 0.3f;
    caster.model_[5] = 0.2f;
    caster.model_[13] = 0.4f;
    caster.model_[14] = 0.5f;
    occluders.draws_ = {caster};
    SceneDrawList cascade_occluders;
    cascade_occluders.projection_[10] = -1;
    SceneDrawList receivers;
    MeshDraw receiver;
    receiver.mesh_ = mesh.Handle();
    receiver.double_sided_ = true;
    receiver.unlit_ = false;
    receiver.roughness_ = 1;
    receivers.draws_ = {receiver};
    receivers.lights_ = {{{0, 0, 1}, {1, 0, 0}}, {{0, 0, 1}, {0, 0, 1}}};
    SceneShadow shadow;
    shadow.depth_ = shadow_depth.Handle();
    shadow.world_to_clip_[10] = -1;
    shadow.resolution_ = 256;
    receivers.shadow_ = shadow;
    const auto capture = [&] {
        renderer.BeginFrame();
        renderer.SubmitSceneDepth(shadow_color.Handle(), shadow_depth.Handle(), occluders);
        if (receivers.shadow_ && receivers.shadow_->cascade_depth_.device_)
            renderer.SubmitSceneDepth(cascade_color.Handle(), cascade_depth.Handle(),
                                      cascade_occluders);
        renderer.SubmitScene(output.Handle(), receivers);
        auto ticket = renderer.RequestReadback(output.Handle());
        renderer.EndFrame();
        for (int i = 0; i < 32; ++i) {
            if (auto image = ticket.Poll()) return image->rgba_;
            renderer.BeginFrame();
            renderer.EndFrame();
        }
        throw std::runtime_error("shadow.readback_timeout");
    };
    constexpr auto kOccluded = (40 * 128 + 64) * 4;
    constexpr auto kLit = (88 * 128 + 64) * 4;
    receivers.shadow_->filter_ = ShadowFilter::kNearest;
    const auto nearest = capture();
    if (nearest[kOccluded] > 5 || nearest[kOccluded + 2] < 60 || nearest[kLit] < 60)
        throw std::runtime_error("shadow.position_direction_and_selected_light");
    if (nearest[kOccluded] > 5) throw std::runtime_error("shadow.hard_filter");
    receivers.shadow_->filter_ = ShadowFilter::kPcf5;
    const auto pcf5 = capture();
    receivers.shadow_->filter_ = ShadowFilter::kPcf13;
    const auto pcf13 = capture();
    const auto red_difference = [](const auto& first, const auto& second) {
        std::uint64_t difference = 0;
        for (std::size_t i = 0; i < first.size(); i += 4) {
            difference += first[i] > second[i] ? first[i] - second[i] : second[i] - first[i];
        }
        return difference;
    };
    // Intermediate means strictly between the occluded and lit levels of the
    // same capture; an absolute upper bound breaks when the lit level or the
    // sub-texel edge alignment shifts (0.8 x lit can exceed any fixed bound).
    const auto fractional_red = [](const auto& image) {
        const int lit = image[kLit];
        std::size_t pixels = 0;
        for (std::size_t i = 0; i < image.size(); i += 4) {
            if (image[i] > 5 && image[i] < lit - 5) ++pixels;
        }
        return pixels;
    };
    const auto nearest_to_pcf5 = red_difference(nearest, pcf5);
    const auto pcf5_to_pcf13 = red_difference(pcf5, pcf13);
    constexpr std::uint64_t kMinFilterEdgeDifference = 256;
    if (nearest_to_pcf5 < kMinFilterEdgeDifference)
        throw std::runtime_error("shadow.pcf5_changes_edge");
    if (pcf5_to_pcf13 < kMinFilterEdgeDifference)
        throw std::runtime_error("shadow.pcf13_changes_edge");
    const auto nearest_fractional = fractional_red(nearest);
    const auto pcf5_fractional = fractional_red(pcf5);
    const auto pcf13_fractional = fractional_red(pcf13);
    if (pcf5_fractional <= nearest_fractional) throw std::runtime_error("shadow.pcf5_softens_edge");
    if (pcf13_fractional <= pcf5_fractional) throw std::runtime_error("shadow.pcf13_softens_edge");
    receivers.shadow_->filter_ = ShadowFilter::kNearest;
    receivers.shadow_->cascade_depth_ = cascade_depth.Handle();
    receivers.shadow_->cascade_world_to_clip_ = receivers.shadow_->world_to_clip_;
    receivers.shadow_->cascade_split_ = 2;
    if (capture()[kOccluded] < 60) throw std::runtime_error("shadow.far_cascade_not_selected");
    receivers.shadow_->cascade_split_ = 4;
    if (capture()[kOccluded] > 5) throw std::runtime_error("shadow.near_cascade_not_selected");
    receivers.shadow_->cascade_depth_ = {};
    receivers.shadow_->cascade_split_ = 0;
    occluders.draws_[0].model_[14] = -0.5f;
    if (capture()[kOccluded] < 60) throw std::runtime_error("shadow.behind_receiver");
    occluders.draws_[0].model_[14] = 0.5f;
    receivers.shadow_->world_to_clip_[12] = 3;
    if (capture()[kOccluded] < 60) throw std::runtime_error("shadow.outside_volume_is_lit");
    receivers.shadow_->world_to_clip_[12] = 0;
    occluders.draws_[0].model_[14] = 0.002f;
    receivers.shadow_->depth_bias_ = 0.002f;
    if (capture()[kOccluded] < 60) throw std::runtime_error("shadow.bias_prevents_acne");
    // Perspective spot projection uses clip W as well as the mapped depth.
    occluders.view_[14] = -3;
    occluders.projection_ = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, -1.020202f, -1, 0, 0, -0.2020202f, 0};
    occluders.draws_[0].model_[14] = 0.5f;
    receivers.lights_.clear();
    PositionalLight spot;
    spot.spot_ = true;
    spot.radiance_ = {9, 0, 0};
    spot.range_ = 100;
    spot.cone_angle_ = 45;
    receivers.positional_lights_ = {spot};
    receivers.shadow_->world_to_clip_ = occluders.projection_;
    receivers.shadow_->world_to_clip_[14] = 2.858586f;
    receivers.shadow_->world_to_clip_[15] = 3;
    const auto perspective = capture();
    if (perspective[kOccluded] > 5 || perspective[kLit] < 50)
        throw std::runtime_error("shadow.perspective_spot");

    std::array<Texture, 6> point_colors;
    std::array<Texture, 6> point_depths;
    for (std::size_t face = 0; face < point_depths.size(); ++face) {
        point_colors[face] = renderer.CreateTexture({256, 256});
        point_depths[face] = renderer.CreateDepthTexture({256, 256});
    }
    constexpr std::array<std::array<float, 3>, 6> kPointPositions{{{0.7f, -0.55f, 0},
                                                                   {-0.7f, -0.55f, 0},
                                                                   {-0.2f, -0.7f, 0},
                                                                   {0.2f, 0.7f, 0},
                                                                   {-0.25f, 0, 0.8f},
                                                                   {0.25f, 0, -0.8f}}};
    constexpr std::array<std::size_t, 6> kPointSamples{(99 * 128 + 109) * 4, (99 * 128 + 19) * 4,
                                                       (109 * 128 + 51) * 4, (19 * 128 + 77) * 4,
                                                       (64 * 128 + 48) * 4,  (64 * 128 + 80) * 4};
    receivers.draws_.clear();
    for (const auto& position : kPointPositions) {
        auto point_receiver = receiver;
        point_receiver.model_[0] = 0.12f;
        point_receiver.model_[5] = 0.12f;
        point_receiver.model_[12] = position[0];
        point_receiver.model_[13] = position[1];
        point_receiver.model_[14] = position[2];
        point_receiver.double_sided_ = false;
        point_receiver.normal_[8] = -position[0];
        point_receiver.normal_[9] = -position[1];
        point_receiver.normal_[10] = -position[2];
        receivers.draws_.push_back(point_receiver);
    }
    PositionalLight point;
    point.radiance_ = {20, 0, 0};
    point.position_ = {0, 0, 0};
    point.range_ = 10;
    point.decay_ = 0;
    receivers.positional_lights_ = {point};
    receivers.shadow_ = SceneShadow{};
    receivers.shadow_->resolution_ = 256;
    receivers.shadow_->filter_ = ShadowFilter::kNearest;
    for (std::size_t face = 0; face < point_depths.size(); ++face) {
        receivers.shadow_->point_depths_[face] = point_depths[face].Handle();
        auto& matrix = receivers.shadow_->point_world_to_clip_[face];
        matrix = {};
        matrix[15] = 1;
        if (face < 2) matrix[2] = face == 0 ? 1.0f : -1.0f;
        if (face >= 2 && face < 4) matrix[6] = face == 2 ? -1.0f : 1.0f;
        if (face >= 4) matrix[10] = face == 4 ? 1.0f : -1.0f;
    }
    SceneDrawList blocked_face;
    auto blocker = caster;
    blocker.model_ = kIdentityMatrix;
    blocker.model_[0] = 2;
    blocker.model_[5] = 2;
    blocked_face.draws_ = {blocker};
    SceneDrawList open_face;
    const auto capture_point = [&](std::size_t blocked) {
        renderer.BeginFrame();
        for (std::size_t face = 0; face < point_depths.size(); ++face)
            renderer.SubmitSceneDepth(point_colors[face].Handle(), point_depths[face].Handle(),
                                      face == blocked ? blocked_face : open_face);
        renderer.SubmitScene(output.Handle(), receivers);
        auto ticket = renderer.RequestReadback(output.Handle());
        renderer.EndFrame();
        for (int i = 0; i < 32; ++i) {
            if (auto image = ticket.Poll()) return image->rgba_;
            renderer.BeginFrame();
            renderer.EndFrame();
        }
        throw std::runtime_error("point_shadow.readback_timeout");
    };
    for (std::size_t blocked = 0; blocked < point_depths.size(); ++blocked) {
        const auto point_pixels = capture_point(blocked);
        for (std::size_t face = 0; face < point_depths.size(); ++face) {
            const auto red = point_pixels[kPointSamples[face]];
            if ((face == blocked && red > 5) || (face != blocked && red < 50))
                throw std::runtime_error("shadow.point_cube_face_selection");
        }
    }
    for (std::size_t face = 0; face < point_depths.size(); ++face)
        receivers.shadow_->point_world_to_clip_[face] = PointShadowMatrix(face);
    receivers.positional_lights_[0].radiance_ = {1, 0, 0};
    struct SeamCase {
        std::array<float, 3> position_{};
        std::size_t row_ = 0;
        std::size_t column_ = 0;
        std::size_t blocked_face_ = 0;
    };
    constexpr float kPrimary = 0.7109375f;
    constexpr float kAdjacent = 0.708f;
    constexpr std::size_t kPositive = 109;
    constexpr std::size_t kNegative = 18;
    constexpr std::size_t kCenter = 64;
    // First twelve cases cover every cube edge; the final eight cover every corner.
    constexpr std::array<SeamCase, 20> kSeams{{
            {{kPrimary, 0, kAdjacent}, kCenter, kPositive, 0},
            {{kPrimary, 0, -kAdjacent}, kCenter, kPositive, 0},
            {{-kPrimary, 0, kAdjacent}, kCenter, kNegative, 1},
            {{-kPrimary, 0, -kAdjacent}, kCenter, kNegative, 1},
            {{kPrimary, kPrimary, 0.2f}, kNegative, kPositive, 0},
            {{kPrimary, -kPrimary, 0.2f}, kPositive, kPositive, 0},
            {{-kPrimary, kPrimary, 0.2f}, kNegative, kNegative, 1},
            {{-kPrimary, -kPrimary, 0.2f}, kPositive, kNegative, 1},
            {{0, kPrimary, kAdjacent}, kNegative, kCenter, 3},
            {{0, kPrimary, -kAdjacent}, kNegative, kCenter, 3},
            {{0, -kPrimary, kAdjacent}, kPositive, kCenter, 2},
            {{0, -kPrimary, -kAdjacent}, kPositive, kCenter, 2},
            {{kPrimary, kPrimary, kAdjacent}, kNegative, kPositive, 0},
            {{kPrimary, kPrimary, -kAdjacent}, kNegative, kPositive, 0},
            {{kPrimary, -kPrimary, kAdjacent}, kPositive, kPositive, 0},
            {{kPrimary, -kPrimary, -kAdjacent}, kPositive, kPositive, 0},
            {{-kPrimary, kPrimary, kAdjacent}, kNegative, kNegative, 1},
            {{-kPrimary, kPrimary, -kAdjacent}, kNegative, kNegative, 1},
            {{-kPrimary, -kPrimary, kAdjacent}, kPositive, kNegative, 1},
            {{-kPrimary, -kPrimary, -kAdjacent}, kPositive, kNegative, 1},
    }};
    std::array<std::uint8_t, 4> representative{};
    for (std::size_t seam_index = 0; seam_index < kSeams.size(); ++seam_index) {
        const auto& seam = kSeams[seam_index];
        auto seam_receiver = receiver;
        seam_receiver.model_[0] = 0.12f;
        seam_receiver.model_[5] = 0.12f;
        seam_receiver.model_[12] = seam.position_[0];
        seam_receiver.model_[13] = seam.position_[1];
        seam_receiver.model_[14] = seam.position_[2];
        seam_receiver.double_sided_ = false;
        seam_receiver.normal_[8] = -seam.position_[0];
        seam_receiver.normal_[9] = -seam.position_[1];
        seam_receiver.normal_[10] = -seam.position_[2];
        receivers.draws_ = {seam_receiver};
        receivers.shadow_->filter_ = ShadowFilter::kNearest;
        const auto seam_nearest = capture_point(seam.blocked_face_);
        receivers.shadow_->filter_ = ShadowFilter::kPcf5;
        const auto filtered_5 = capture_point(seam.blocked_face_);
        receivers.shadow_->filter_ = ShadowFilter::kPcf13;
        const auto filtered_13 = capture_point(seam.blocked_face_);
        const auto open = capture_point(seam.blocked_face_ ^ 1);
        // The shadow boundary can drift by a sub-texel amount with the host
        // shader compiler; scan a small window around the nominal seam sample
        // for the transition band instead of pinning one exact pixel.
        bool filtered = false;
        for (int dr = -3; dr <= 3 && !filtered; ++dr) {
            for (int dc = -3; dc <= 3 && !filtered; ++dc) {
                const auto row = std::clamp<int>(int(seam.row_) + dr, 0, 127);
                const auto column = std::clamp<int>(int(seam.column_) + dc, 0, 127);
                const auto sample = (row * 128 + column) * 4;
                filtered = seam_nearest[sample] <= 5 && open[sample] >= 50 &&
                           filtered_5[sample] > 5 && filtered_13[sample] > 5 &&
                           filtered_5[sample] < open[sample] - 5 &&
                           filtered_13[sample] < open[sample] - 5;
                if (seam_index == 0 && dr == 0 && dc == 0)
                    representative = {seam_nearest[sample], filtered_5[sample],
                                      filtered_13[sample], open[sample]};
            }
        }
        if (!filtered)
            throw std::runtime_error("shadow.point_cube_cross_face_filter_" +
                                     std::to_string(seam_index));
    }
    std::cout << "Point shadow seams: 12 edges and 8 corners passed; representative nearest="
              << unsigned(representative[0]) << " pcf5=" << unsigned(representative[1])
              << " pcf13=" << unsigned(representative[2]) << " open=" << unsigned(representative[3])
              << '\n';
    receivers.shadow_.reset();
    receivers.draws_ = {receiver};
    receivers.positional_lights_ = {spot};
    if (capture()[kOccluded] < 60) throw std::runtime_error("shadow.disable_releases_binding");
    std::cout
            << "Shadows: sampled depth occlusion, Y orientation, selected light, bounds, bias and "
               "Nearest/PCF5/PCF13 edge differences plus six point cube faces and cross-face "
               "filtering passed ("
            << nearest_to_pcf5 << ", " << pcf5_to_pcf13 << "; fractional " << nearest_fractional
            << ", " << pcf5_fractional << ", " << pcf13_fractional << "; seam "
            << unsigned(representative[0]) << ", " << unsigned(representative[1]) << ", "
            << unsigned(representative[2]) << ", " << unsigned(representative[3]) << ")\n";
}
}  // namespace rhythm::validation
