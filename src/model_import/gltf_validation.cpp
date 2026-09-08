#include <stdexcept>
#include <string_view>

#include "gltf_internal.h"

namespace rhythm::model_import::detail {
void Require(bool condition, const char* message) {
    if (!condition) throw std::invalid_argument(message);
}
void Preflight(std::span<const std::uint8_t> bytes) {
    Require(bytes.size() >= 20 && bytes.size() <= 64 * 1024 * 1024, "gltf.file_limit");
    const auto word = [&](std::size_t offset) {
        return std::uint32_t(bytes[offset]) | std::uint32_t(bytes[offset + 1]) << 8 |
               std::uint32_t(bytes[offset + 2]) << 16 | std::uint32_t(bytes[offset + 3]) << 24;
    };
    const auto json_length = word(12);
    Require(word(0) == 0x46546c67 && word(4) == 2 && word(8) == bytes.size() &&
                    word(16) == 0x4e4f534a && json_length <= 8 * 1024 * 1024 &&
                    json_length <= bytes.size() - 20 && json_length % 4 == 0,
            "gltf.header");
    bool quoted = false, escaped = false;
    int depth = 0;
    for (const auto c : bytes.subspan(20, json_length)) {
        if (quoted) {
            if (escaped)
                escaped = false;
            else if (c == '\\')
                escaped = true;
            else if (c == '"')
                quoted = false;
        } else if (c == '"')
            quoted = true;
        else if (c == '{' || c == '[')
            Require(++depth <= 64, "gltf.json_depth");
        else if (c == '}' || c == ']')
            Require(--depth >= 0, "gltf.json_depth");
    }
    Require(!quoted && depth == 0, "gltf.json");
}
void ValidateProfile(const cgltf_data& data) {
    Require(data.file_type == cgltf_file_type_glb && data.buffers_count == 1 && data.buffers &&
                    !data.buffers[0].uri && data.bin && data.buffers[0].size <= data.bin_size,
            "gltf.embedded_buffer");
    Require(data.nodes_count > 0 && data.nodes_count <= 2048 && data.meshes_count <= 512 &&
                    data.materials_count < 128 && data.accessors_count <= 4096 &&
                    data.buffer_views_count <= 4096 && data.scenes_count <= 64,
            "gltf.model_limit");
    Require(data.images_count <= 64 && data.textures_count <= 128 && data.samplers_count <= 128,
            "gltf.image_count");
    Require(data.skins_count <= 64 && data.animations_count <= 64 && !data.variants_count &&
                    !data.cameras_count && !data.lights_count,
            "gltf.static_profile");
    for (std::size_t i = 0; i < data.extensions_required_count; ++i)
        Require(data.extensions_required[i] &&
                        std::string_view(data.extensions_required[i]) == "KHR_materials_unlit",
                "gltf.required_extension");
    for (std::size_t i = 0; i < data.buffer_views_count; ++i) {
        const auto& view = data.buffer_views[i];
        Require(view.buffer && view.offset <= view.buffer->size &&
                        view.size <= view.buffer->size - view.offset &&
                        !view.has_meshopt_compression,
                "gltf.buffer_view");
    }
    // Check arithmetic before cgltf_validate reads any index/accessor bytes.
    for (std::size_t i = 0; i < data.accessors_count; ++i) {
        const auto& accessor = data.accessors[i];
        Require(!accessor.is_sparse && accessor.count > 0 && accessor.count <= 750000 &&
                        accessor.buffer_view && accessor.stride > 0 && accessor.stride <= 252,
                "gltf.accessor_profile");
        const auto element_size = cgltf_calc_size(accessor.type, accessor.component_type);
        const auto size = accessor.buffer_view->size;
        Require(element_size > 0 && accessor.stride >= element_size && accessor.offset <= size &&
                        element_size <= size - accessor.offset &&
                        accessor.count - 1 <=
                                (size - accessor.offset - element_size) / accessor.stride,
                "gltf.accessor_bounds");
    }
    std::size_t primitives = 0;
    for (std::size_t i = 0; i < data.meshes_count; ++i) {
        Require(data.meshes[i].primitives_count <= 512 - primitives &&
                        !data.meshes[i].weights_count,
                "gltf.mesh_limit");
        primitives += data.meshes[i].primitives_count;
        for (std::size_t j = 0; j < data.meshes[i].primitives_count; ++j) {
            const auto& primitive = data.meshes[i].primitives[j];
            Require(primitive.type == cgltf_primitive_type_triangles && !primitive.targets_count &&
                            !primitive.has_draco_mesh_compression && !primitive.mappings_count &&
                            primitive.attributes_count > 0 && primitive.attributes_count <= 6,
                    "gltf.primitive_profile");
        }
    }
    for (std::size_t i = 0; i < data.nodes_count; ++i)
        Require(!data.nodes[i].has_mesh_gpu_instancing && !data.nodes[i].weights_count,
                "gltf.node_profile");
}
}  // namespace rhythm::model_import::detail
