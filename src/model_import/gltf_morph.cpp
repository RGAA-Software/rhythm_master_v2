#include <algorithm>
#include <array>
#include <set>
#include <stdexcept>

#include "gltf_internal.h"

namespace rhythm::model_import::detail {
std::size_t MorphCount(const cgltf_node& node) {
    return node.mesh && node.mesh->primitives_count ? node.mesh->primitives[0].targets_count : 0;
}
scene::NodePose ReadNodePose(const cgltf_node& node) {
    scene::NodePose pose;
    pose.translation_ = {node.translation[0], node.translation[1], node.translation[2]};
    pose.rotation_ = {node.rotation[0], node.rotation[1], node.rotation[2], node.rotation[3]};
    pose.scale_ = {node.scale[0], node.scale[1], node.scale[2]};
    if (node.has_matrix) {
        pose.matrix_.emplace();
        std::copy(std::begin(node.matrix), std::end(node.matrix), pose.matrix_->values_.begin());
    }
    const auto count = MorphCount(node);
    Require(count <= 4 && (!node.weights_count || node.weights_count == count),
            "gltf.morph_weights");
    if (node.weights_count) {
        for (std::size_t i = 0; i < count; ++i) pose.weights_[i] = node.weights[i];
    } else if (node.mesh && node.mesh->weights_count) {
        Require(node.mesh->weights_count == count, "gltf.morph_weights");
        for (std::size_t i = 0; i < count; ++i) pose.weights_[i] = node.mesh->weights[i];
    }
    return pose;
}
void ReadMorphs(const cgltf_primitive& primitive, scene::Mesh& mesh, bool normal, bool tangent,
                std::stop_token stop) {
    Require(primitive.targets_count <= 4, "gltf.morph_count");
    for (std::size_t target_index = 0; target_index < primitive.targets_count; ++target_index) {
        const auto& source = primitive.targets[target_index];
        Require(source.attributes_count > 0 && source.attributes_count <= 3,
                "gltf.morph_attributes");
        scene::Mesh::MorphTarget target;
        target.deltas_.resize(mesh.vertices_.size());
        std::set<cgltf_attribute_type> types;
        for (std::size_t attribute_index = 0; attribute_index < source.attributes_count;
             ++attribute_index) {
            const auto& attribute = source.attributes[attribute_index];
            Require(attribute.data && attribute.index == 0 && types.insert(attribute.type).second &&
                            (attribute.type == cgltf_attribute_type_position ||
                             (attribute.type == cgltf_attribute_type_normal && normal) ||
                             (attribute.type == cgltf_attribute_type_tangent && tangent)),
                    "gltf.morph_attributes");
            const auto& accessor = *attribute.data;
            Require(accessor.count == mesh.vertices_.size() && accessor.type == cgltf_type_vec3 &&
                            accessor.component_type == cgltf_component_type_r_32f &&
                            !accessor.normalized,
                    "gltf.morph_accessor");
            for (std::size_t i = 0; i < target.deltas_.size(); ++i) {
                if (i % 4096 == 0 && stop.stop_requested())
                    throw std::runtime_error("gltf.cancelled");
                auto& delta = target.deltas_[i];
                auto& values = attribute.type == cgltf_attribute_type_position ? delta.position_
                               : attribute.type == cgltf_attribute_type_normal ? delta.normal_
                                                                               : delta.tangent_;
                Require(cgltf_accessor_read_float(&accessor, i, values.data(), 3),
                        "gltf.morph_accessor");
            }
        }
        mesh.morphs_.push_back(std::move(target));
    }
}
}  // namespace rhythm::model_import::detail
