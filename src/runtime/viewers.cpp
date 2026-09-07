#include "rhythm/runtime/viewers.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "point_ops.h"
#include "rhythm/render/layout.h"
#include "scene_ops.h"
#include "scene_pass.h"

namespace rhythm::runtime {
namespace detail {
class ScenePreviews final {
   public:
    struct Entry {
        ScenePass pass_{};
        render::Texture target_{};
        render::Extent extent_{};
    };
    std::map<graph::NodeId, Entry> entries_{};
};
}  // namespace detail
Viewers::Viewers() = default;
Viewers::~Viewers() = default;
bool Viewers::BeginFrame(double seconds, bool visible, std::uint64_t reset_generation) {
    if (!std::isfinite(seconds) || seconds < 0) throw std::invalid_argument("viewer.time");
    if (!visible || reset_generation != reset_generation_ ||
        (last_capture_ && seconds < *last_capture_)) {
        textures_.clear();
        point_sprite_ = {};
        scenes_.reset();
        outputs_.clear();
        last_capture_.reset();
    }
    reset_generation_ = reset_generation;
    due_ = visible && (!last_capture_ || seconds - *last_capture_ + 1e-9 >= 1.0 / 15.0);
    if (due_) last_capture_ = seconds;
    return due_;
}
void Viewers::Capture(const FrameResult& frame, std::span<const graph::NodeId> nodes,
                      render::Renderer& renderer) {
    if (nodes.size() > kMaxPreviews) throw std::length_error("viewer.limit");
    if (!due_) return;
    due_ = false;
    outputs_.clear();
    if (scenes_)
        std::erase_if(scenes_->entries_, [&](const auto& entry) {
            return std::find(nodes.begin(), nodes.end(), entry.first) == nodes.end();
        });
    std::size_t count = 0;
    for (const auto node : nodes) {
        const auto source = std::find_if(frame.outputs_.begin(), frame.outputs_.end(),
                                         [&](const auto& item) { return item.node_ == node; });
        if (source == frame.outputs_.end() ||
            (!source->points_ && !source->geometry_ && !source->scene_ && !source->material_ &&
             !renderer.IsValid(source->texture_)))
            continue;
        if (count == textures_.size()) textures_.push_back(renderer.CreateTexture({256, 144}));
        auto& texture = textures_[count++];
        render::DrawList draw;
        draw.width_ = 256;
        draw.height_ = 144;
        const auto fit = render::AspectFit(frame.extent_, {0, 0, 256, 144});
        auto source_texture = source->texture_;
        if (source->geometry_ || source->scene_ || source->material_) {
            if (!scenes_) scenes_ = std::make_unique<detail::ScenePreviews>();
            auto& entry = scenes_->entries_[node];
            const render::Extent extent{
                    static_cast<std::uint16_t>(std::max(1.0f, std::round(fit.width_))),
                    static_cast<std::uint16_t>(std::max(1.0f, std::round(fit.height_)))};
            if (entry.extent_ != extent || !renderer.IsValid(entry.target_.Handle())) {
                entry = {};
                entry.target_ = renderer.CreateTexture(extent);
                entry.extent_ = extent;
            }
            const auto scene = detail::PreviewScene(*source);
            const auto list = entry.pass_.Build(scene, scene::Camera{}, extent, renderer);
            renderer.SubmitScene(entry.target_.Handle(), list);
            source_texture = entry.target_.Handle();
        } else if (scenes_)
            scenes_->entries_.erase(node);
        if (source->points_) {
            if (!renderer.IsValid(point_sprite_.Handle()))
                point_sprite_ = detail::CreatePointSprite(renderer);
            draw.width_ = fit.width_;
            draw.height_ = fit.height_;
            detail::DrawPoints(*source->points_, point_sprite_.Handle(),
                               render::BlendMode::kSourceOver, draw);
            for (auto& vertex : draw.vertices_) {
                vertex.x_ += fit.x_;
                vertex.y_ += fit.y_;
            }
            for (auto& command : draw.commands_) command.clip_ = fit;
            draw.width_ = 256;
            draw.height_ = 144;
        } else {
            draw.vertices_ = {{fit.x_, fit.y_, 0, 0},
                              {fit.x_ + fit.width_, fit.y_, 1, 0},
                              {fit.x_ + fit.width_, fit.y_ + fit.height_, 1, 1},
                              {fit.x_, fit.y_ + fit.height_, 0, 1}};
            draw.indices_ = {0, 1, 2, 0, 2, 3};
            draw.commands_ = {{source_texture, 0, 6, {0, 0, 256, 144}}};
        }
        renderer.Submit(texture.Handle(), draw, 0x000000ff);
        outputs_.push_back({node, 0, texture.Handle(), source->version_});
    }
    textures_.resize(count);
}
}  // namespace rhythm::runtime
