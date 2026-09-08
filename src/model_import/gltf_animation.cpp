#include <array>
#include <stdexcept>
#include <string_view>

#include "gltf_internal.h"

namespace rhythm::model_import::detail {
namespace {
scene::AnimationInterpolation Interpolation(cgltf_interpolation_type value) {
    switch (value) {
        case cgltf_interpolation_type_step:
            return scene::AnimationInterpolation::kStep;
        case cgltf_interpolation_type_linear:
            return scene::AnimationInterpolation::kLinear;
        case cgltf_interpolation_type_cubic_spline:
            return scene::AnimationInterpolation::kCubicSpline;
        default:
            throw std::invalid_argument("gltf.animation_interpolation");
    }
}
scene::AnimationProperty Property(cgltf_animation_path_type value) {
    switch (value) {
        case cgltf_animation_path_type_translation:
            return scene::AnimationProperty::kTranslation;
        case cgltf_animation_path_type_rotation:
            return scene::AnimationProperty::kRotation;
        case cgltf_animation_path_type_scale:
            return scene::AnimationProperty::kScale;
        default:
            throw std::invalid_argument("gltf.animation_path");
    }
}
}  // namespace
void ReadAnimations(const cgltf_data& data, scene::Model& model, std::stop_token stop) {
    std::size_t tracks = 0, values = 0;
    for (std::size_t clip_index = 0; clip_index < data.animations_count; ++clip_index) {
        const auto& source = data.animations[clip_index];
        Require(source.channels_count <= 8192 - tracks && source.samplers_count <= 8192,
                "gltf.animation_budget");
        tracks += source.channels_count;
        std::vector<scene::AnimationTrack> channels;
        for (std::size_t channel_index = 0; channel_index < source.channels_count;
             ++channel_index) {
            if (stop.stop_requested()) throw std::runtime_error("gltf.cancelled");
            const auto& channel = source.channels[channel_index];
            Require(channel.target_node && channel.sampler && !channel.target_node->has_matrix,
                    "gltf.animation_target");
            const auto& sampler = *channel.sampler;
            Require(sampler.input && sampler.output, "gltf.animation_sampler");
            const auto& input = *sampler.input;
            const auto& output = *sampler.output;
            scene::AnimationTrack track;
            track.node_ = cgltf_node_index(&data, channel.target_node) + 1;
            track.property_ = Property(channel.target_path);
            track.interpolation_ = Interpolation(sampler.interpolation);
            track.components_ = track.property_ == scene::AnimationProperty::kRotation ? 4 : 3;
            const auto stride = sampler.interpolation == cgltf_interpolation_type_cubic_spline
                                        ? std::size_t{3}
                                        : std::size_t{1};
            Require(input.type == cgltf_type_scalar &&
                            input.component_type == cgltf_component_type_r_32f &&
                            !input.normalized && input.count <= 262144 &&
                            output.component_type == cgltf_component_type_r_32f &&
                            !output.normalized &&
                            output.type ==
                                    (track.components_ == 4 ? cgltf_type_vec4 : cgltf_type_vec3) &&
                            output.count == input.count * stride && output.count <= 262144 - values,
                    "gltf.animation_accessor");
            values += output.count;
            track.times_.reserve(input.count);
            for (std::size_t i = 0; i < input.count; ++i) {
                float value = 0;
                Require(cgltf_accessor_read_float(&input, i, &value, 1), "gltf.animation_time");
                track.times_.push_back(value);
            }
            track.values_.reserve(output.count);
            for (std::size_t i = 0; i < output.count; ++i) {
                std::array<float, 4> value{};
                Require(cgltf_accessor_read_float(&output, i, value.data(), track.components_),
                        "gltf.animation_value");
                track.values_.push_back({value[0], value[1], value[2], value[3]});
            }
            const auto& node = *channel.target_node;
            scene::NodePose rest;
            rest.translation_ = {node.translation[0], node.translation[1], node.translation[2]};
            rest.rotation_ = {node.rotation[0], node.rotation[1], node.rotation[2],
                              node.rotation[3]};
            rest.scale_ = {node.scale[0], node.scale[1], node.scale[2]};
            model.rest_pose_.emplace(track.node_, rest);
            channels.push_back(std::move(track));
        }
        Require(!source.name || std::string_view(source.name).size() <= 1024,
                "gltf.animation_name");
        model.animations_.emplace_back(source.name ? source.name : "", std::move(channels));
    }
}
}  // namespace rhythm::model_import::detail
