#include <array>
#include <cmath>
#include <functional>
#include <optional>
#include <stdexcept>

#include "gltf_internal.h"

namespace rhythm::model_import::detail {
scene::Mesh ReadMesh(const cgltf_data& data, const cgltf_primitive& primitive,
                     std::size_t& vertices, std::size_t& indices, std::stop_token stop) {
    // All native references/pointers are checked borrowed data scoped to this
    // synchronous conversion; no parser storage reaches the returned mesh.
    std::optional<std::reference_wrapper<const cgltf_accessor>> position, normal, uv;
    for (std::size_t i = 0; i < primitive.attributes_count; ++i) {
        const auto& attribute = primitive.attributes[i];
        Require(attribute.data && attribute.index == 0, "gltf.attribute");
        if (attribute.type == cgltf_attribute_type_position) {
            Require(!position);
            position = *attribute.data;
        } else if (attribute.type == cgltf_attribute_type_normal) {
            Require(!normal);
            normal = *attribute.data;
        } else if (attribute.type == cgltf_attribute_type_texcoord) {
            Require(!uv);
            uv = *attribute.data;
        } else
            Require(false, "gltf.attribute_profile");
    }
    Require(position.has_value(), "gltf.position");
    const auto& positions = position->get();
    Require(positions.type == cgltf_type_vec3 &&
                    positions.component_type == cgltf_component_type_r_32f &&
                    positions.count <= 250000 - vertices,
            "gltf.vertex_limit");
    if (normal)
        Require(normal->get().type == cgltf_type_vec3 && normal->get().count == positions.count);
    if (uv) Require(uv->get().type == cgltf_type_vec2 && uv->get().count == positions.count);
    const auto count = primitive.indices ? primitive.indices->count : positions.count;
    Require(count > 0 && count % 3 == 0 && count <= 750000 - indices, "gltf.index_limit");
    vertices += positions.count;
    indices += count;
    scene::Mesh mesh;
    mesh.material_ = primitive.material
                             ? static_cast<std::uint32_t>(
                                       cgltf_material_index(&data, primitive.material) + 1)
                             : 0;
    mesh.vertices_.reserve(positions.count);
    for (std::size_t i = 0; i < positions.count; ++i) {
        if (i % 4096 == 0 && stop.stop_requested()) throw std::runtime_error("gltf.cancelled");
        std::array<float, 3> p{}, n{0, 0, 1};
        std::array<float, 2> tex{};
        Require(cgltf_accessor_read_float(&positions, i, p.data(), p.size()));
        if (normal) Require(cgltf_accessor_read_float(&normal->get(), i, n.data(), n.size()));
        if (uv) Require(cgltf_accessor_read_float(&uv->get(), i, tex.data(), tex.size()));
        const auto normalized = scene::Normalize({n[0], n[1], n[2]});
        mesh.vertices_.push_back({p[0], p[1], p[2], static_cast<float>(normalized.x_),
                                  static_cast<float>(normalized.y_),
                                  static_cast<float>(normalized.z_), tex[0], tex[1]});
    }
    mesh.indices_.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const auto index = primitive.indices ? cgltf_accessor_read_index(primitive.indices, i) : i;
        Require(index < positions.count, "gltf.index");
        mesh.indices_.push_back(static_cast<std::uint32_t>(index));
    }
    if (!normal) scene::GenerateNormals(mesh);
    return mesh;
}
}  // namespace rhythm::model_import::detail
