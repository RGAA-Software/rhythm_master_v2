#include <algorithm>
#include <array>
#include <memory>
#include <memory_resource>
#include <set>
#include <stdexcept>
#include <string_view>

#include "gltf_internal.h"

namespace rhythm::model_import {
namespace {
struct ParserDelete {
    void operator()(cgltf_data* data) const noexcept {
        if (data) cgltf_free(data);
    }
};
void* Allocate(void* user, cgltf_size size) noexcept {
    // Borrowed only while ReadGlb's arena and parsed data are both alive.
    try {
        return static_cast<std::pmr::monotonic_buffer_resource*>(user)->allocate(size);
    } catch (...) {
        return nullptr;
    }
}
void Release(void*, void*) noexcept {
    // The bounded monotonic arena owns every allocation and releases as one RAII value.
}
scene::Material ReadMaterial(const cgltf_material& source) {
    detail::Require(source.alpha_mode == cgltf_alpha_mode_opaque &&
                            !source.has_pbr_specular_glossiness && !source.has_clearcoat &&
                            !source.has_transmission && !source.has_volume && !source.has_ior &&
                            !source.has_specular && !source.has_sheen &&
                            !source.has_emissive_strength && !source.has_iridescence &&
                            !source.has_diffuse_transmission && !source.has_anisotropy &&
                            !source.has_dispersion,
                    "gltf.material_profile");
    scene::Material material;
    const auto& factor = source.pbr_metallic_roughness.base_color_factor;
    material.base_color_ = {factor[0], factor[1], factor[2], 1};
    material.metallic_ = source.pbr_metallic_roughness.metallic_factor;
    material.roughness_ = source.pbr_metallic_roughness.roughness_factor;
    material.emissive_ = {source.emissive_factor[0], source.emissive_factor[1],
                          source.emissive_factor[2]};
    material.unlit_ = source.unlit != 0;
    material.double_sided_ = source.double_sided != 0;
    return material;
}
}  // namespace
scene::Model ReadGlb(std::span<const std::uint8_t> bytes, std::stop_token stop) {
    if (stop.stop_requested()) throw std::runtime_error("gltf.cancelled");
    detail::Preflight(bytes);
    std::vector<std::byte> storage(64 * 1024 * 1024);
    std::pmr::monotonic_buffer_resource arena(storage.data(), storage.size(),
                                              std::pmr::null_memory_resource());
    cgltf_options options{};
    options.type = cgltf_file_type_glb;
    options.memory = {Allocate, Release, &arena};
    cgltf_data* parsed = nullptr;  // C out parameter, immediately transferred to the unique owner.
    const auto result = cgltf_parse(&options, bytes.data(), bytes.size(), &parsed);
    std::unique_ptr<cgltf_data, ParserDelete> owner(parsed);
    detail::Require(result == cgltf_result_success && owner, "gltf.parse");
    const auto& data = *owner;
    detail::ValidateProfile(data);
    // The checked profile has exactly one URI-free GLB buffer. This call binds
    // the borrowed BIN bytes; it cannot resolve an external file or URL.
    detail::Require(cgltf_load_buffers(&options, owner.get(), nullptr) == cgltf_result_success,
                    "gltf.buffer");
    detail::Require(cgltf_validate(owner.get()) == cgltf_result_success, "gltf.validation");
    if (stop.stop_requested()) throw std::runtime_error("gltf.cancelled");
    scene::Model model;
    model.images_ = detail::ReadImages(data, stop);
    model.materials_.emplace_back();
    model.materials_[0].unlit_ = false;
    model.materials_[0].metallic_ = model.materials_[0].roughness_ = 1;
    for (std::size_t i = 0; i < data.materials_count; ++i) {
        auto material = ReadMaterial(data.materials[i]);
        detail::ReadMaterialTextures(data, data.materials[i], material, model);
        model.materials_.push_back(std::move(material));
    }
    std::vector<std::vector<std::uint32_t>> meshes(data.meshes_count);
    std::size_t vertex_count = 0, index_count = 0;
    for (std::size_t i = 0; i < data.meshes_count; ++i)
        for (std::size_t j = 0; j < data.meshes[i].primitives_count; ++j) {
            meshes[i].push_back(static_cast<std::uint32_t>(model.meshes_.size()));
            model.meshes_.push_back(detail::ReadMesh(data, data.meshes[i].primitives[j],
                                                     vertex_count, index_count, stop));
        }
    std::set<scene::NodeId> selected_roots;
    if (data.scene || data.scenes_count) {
        const auto& selected = data.scene ? *data.scene : data.scenes[0];
        for (std::size_t i = 0; i < selected.nodes_count; ++i) {
            detail::Require(selected.nodes[i] != nullptr);
            selected_roots.insert(cgltf_node_index(&data, selected.nodes[i]) + 1);
        }
    } else {
        for (std::size_t i = 0; i < data.nodes_count; ++i)
            if (!data.nodes[i].parent) selected_roots.insert(i + 1);
    }
    model.nodes_.reserve(data.nodes_count);
    for (std::size_t i = 0; i < data.nodes_count; ++i) {
        const auto& source = data.nodes[i];
        scene::Node node;
        node.id_ = i + 1;
        if (source.parent) node.parent_ = cgltf_node_index(&data, source.parent) + 1;
        node.visible_ = node.parent_.has_value() || selected_roots.contains(node.id_);
        if (source.name) {
            detail::Require(std::string_view(source.name).size() <= 1024, "gltf.node_name");
            node.name_ = source.name;
        }
        std::array<float, 16> local{};
        cgltf_node_transform_local(&source, local.data());
        std::copy(local.begin(), local.end(), node.local_.values_.begin());
        if (source.mesh) node.meshes_ = meshes.at(cgltf_mesh_index(&data, source.mesh));
        model.nodes_.push_back(std::move(node));
    }
    detail::ReadAnimations(data, model, stop);
    scene::Validate(model);
    return model;
}
}  // namespace rhythm::model_import
