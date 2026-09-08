#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <optional>
#include <stdexcept>

#include "gltf_internal.h"

namespace rhythm::model_import::detail {
std::vector<scene::Mesh::JointWeights> ReadSkinWeights(const cgltf_primitive& primitive,
                                                       std::size_t count, std::stop_token stop) {
    std::optional<std::reference_wrapper<const cgltf_accessor>> joints, weights;
    for (std::size_t i = 0; i < primitive.attributes_count; ++i) {
        const auto& attribute = primitive.attributes[i];
        if (attribute.type == cgltf_attribute_type_joints) {
            Require(attribute.data && attribute.index == 0 && !joints, "gltf.skin_attributes");
            joints = *attribute.data;
        }
        if (attribute.type == cgltf_attribute_type_weights) {
            Require(attribute.data && attribute.index == 0 && !weights, "gltf.skin_attributes");
            weights = *attribute.data;
        }
    }
    Require(joints.has_value() == weights.has_value(), "gltf.skin_attributes");
    if (!joints) return {};
    const auto& j = joints->get();
    const auto& w = weights->get();
    Require(j.type == cgltf_type_vec4 && w.type == cgltf_type_vec4 && j.count == count &&
                    w.count == count && count <= 250000 && !j.normalized &&
                    (j.component_type == cgltf_component_type_r_8u ||
                     j.component_type == cgltf_component_type_r_16u) &&
                    ((w.component_type == cgltf_component_type_r_32f && !w.normalized) ||
                     ((w.component_type == cgltf_component_type_r_8u ||
                       w.component_type == cgltf_component_type_r_16u) &&
                      w.normalized)),
            "gltf.skin_accessor");
    std::vector<scene::Mesh::JointWeights> result;
    result.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        if (i % 4096 == 0 && stop.stop_requested()) throw std::runtime_error("gltf.cancelled");
        std::array<cgltf_uint, 4> indices{};
        std::array<float, 4> values{};
        Require(cgltf_accessor_read_uint(&j, i, indices.data(), 4) &&
                        cgltf_accessor_read_float(&w, i, values.data(), 4),
                "gltf.skin_accessor");
        scene::Mesh::JointWeights vertex;
        float sum = 0;
        for (std::size_t k = 0; k < 4; ++k) {
            Require(indices[k] < scene::kMaximumModelSkinBones && std::isfinite(values[k]) &&
                            values[k] >= 0 && values[k] <= 1,
                    "gltf.skin_weights");
            vertex.joints_[k] = static_cast<std::uint8_t>(indices[k]);
            sum += values[k];
        }
        Require(sum > 1e-8f, "gltf.skin_weights");
        for (std::size_t k = 0; k < 4; ++k) vertex.weights_[k] = values[k] / sum;
        result.push_back(vertex);
    }
    return result;
}
void ReadSkins(const cgltf_data& data, scene::Model& model, std::stop_token stop) {
    Require(data.skins_count <= 64, "gltf.skin_count");
    for (std::size_t i = 0; i < data.skins_count; ++i) {
        if (stop.stop_requested()) throw std::runtime_error("gltf.cancelled");
        const auto& source = data.skins[i];
        Require(source.joints_count > 0 && source.joints_count <= scene::kMaximumModelSkinBones,
                "gltf.skin_bones");
        scene::Skin skin;
        skin.inverse_bind_.resize(source.joints_count);
        for (std::size_t k = 0; k < source.joints_count; ++k) {
            Require(source.joints[k] != nullptr, "gltf.skin_joint");
            skin.joints_.push_back(cgltf_node_index(&data, source.joints[k]) + 1);
        }
        if (source.inverse_bind_matrices) {
            const auto& accessor = *source.inverse_bind_matrices;
            Require(accessor.type == cgltf_type_mat4 &&
                            accessor.component_type == cgltf_component_type_r_32f &&
                            !accessor.normalized && accessor.count == source.joints_count,
                    "gltf.skin_bind");
            for (std::size_t k = 0; k < source.joints_count; ++k) {
                std::array<float, 16> matrix{};
                Require(cgltf_accessor_read_float(&accessor, k, matrix.data(), 16),
                        "gltf.skin_bind");
                std::copy(matrix.begin(), matrix.end(), skin.inverse_bind_[k].values_.begin());
            }
        }
        model.skins_.push_back(std::move(skin));
    }
}
}  // namespace rhythm::model_import::detail
