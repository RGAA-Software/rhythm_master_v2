#include "environment_pass.h"

#include <algorithm>
#include <stdexcept>

namespace rhythm::runtime::detail {
void EnvironmentPass::Apply(const std::optional<scene::EnvironmentSettings>& environment,
                            std::span<const NodeOutput> outputs, render::SceneDrawList& receivers,
                            render::Renderer& renderer) {
    if (!environment) {
        atlas_ = {};
        key_.reset();
        receivers.environment_.reset();
        return;
    }
    const auto& settings = *environment;
    const auto source = std::find_if(outputs.begin(), outputs.end(), [&](const auto& output) {
        return output.node_ == settings.texture_node_;
    });
    if (source == outputs.end() || !renderer.IsValid(source->texture_))
        throw std::invalid_argument("runtime.environment_texture");
    const Key key{source->texture_, source->version_, renderer.Stats().presentation_generation_,
                  settings.source_srgb_};
    if (!renderer.IsValid(atlas_.Handle())) {
        atlas_ = renderer.CreateTexture(render::kEnvironmentAtlasExtent, {},
                                        render::TexturePrecision::kFloat16);
        key_.reset();
    }
    if (key_ != key) {
        render::DrawList list;
        list.width_ = render::kEnvironmentAtlasExtent.width_;
        list.height_ = render::kEnvironmentAtlasExtent.height_;
        list.vertices_ = {{0, 0, 0, 0},
                          {list.width_, 0, 1, 0},
                          {list.width_, list.height_, 1, 1},
                          {0, list.height_, 0, 1}};
        list.indices_ = {0, 1, 2, 0, 2, 3};
        list.commands_ = {{source->texture_, 0, 6, {0, 0, list.width_, list.height_}}};
        list.commands_[0].environment_filter_ = render::EnvironmentFilter{settings.source_srgb_};
        renderer.Submit(atlas_.Handle(), list);
        key_ = key;
    }
    receivers.environment_ =
            render::SceneEnvironment{atlas_.Handle(), settings.energy_, settings.rotation_};
}
}  // namespace rhythm::runtime::detail
