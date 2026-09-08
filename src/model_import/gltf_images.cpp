#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string_view>

#include "gltf_internal.h"

#if defined(RHYTHM_HAS_IMAGE_DECODER)
#include "rhythm/media/video_decoder.h"
#endif

namespace rhythm::model_import::detail {
namespace {
std::optional<std::uint32_t> ImageIndex(const cgltf_data& data, const cgltf_texture_view& view) {
    // All cgltf pointers are checked borrowed values within this synchronous adapter.
    if (!view.texture) return {};
    const auto& texture = *view.texture;
    Require(view.texcoord == 0 && !view.has_transform && texture.image && !texture.has_basisu &&
                    !texture.has_webp,
            "gltf.texture_profile");
    if (texture.sampler) {
        const auto& sampler = *texture.sampler;
        Require(sampler.wrap_s == cgltf_wrap_mode_repeat &&
                        sampler.wrap_t == cgltf_wrap_mode_repeat &&
                        (sampler.mag_filter == cgltf_filter_type_undefined ||
                         sampler.mag_filter == cgltf_filter_type_linear) &&
                        (sampler.min_filter == cgltf_filter_type_undefined ||
                         sampler.min_filter == cgltf_filter_type_linear),
                "gltf.sampler_profile");
    }
    const auto index = cgltf_image_index(&data, texture.image);
    Require(index < data.images_count, "gltf.image_index");
    return std::uint32_t(index);
}
std::size_t ImageBytes(const scene::Model& model) {
    std::size_t bytes = 0;
    for (const auto& image : model.images_) {
        Require(image.rgba_.size() <= scene::kMaximumModelImageBytes - bytes, "gltf.image_budget");
        bytes += image.rgba_.size();
    }
    return bytes;
}
}  // namespace
std::vector<scene::TextureImage> ReadImages(const cgltf_data& data, std::stop_token stop) {
    std::vector<scene::TextureImage> images;
#if defined(RHYTHM_HAS_IMAGE_DECODER)
    std::size_t total = 0;
#endif
    for (std::size_t i = 0; i < data.images_count; ++i) {
        if (stop.stop_requested()) throw std::runtime_error("gltf.cancelled");
        const auto& image = data.images[i];
        Require(!image.uri && image.buffer_view && image.mime_type, "gltf.embedded_image");
        const std::string_view mime(image.mime_type);
        Require(mime == "image/png" || mime == "image/jpeg", "gltf.image_type");
        const auto& view = *image.buffer_view;
        Require(view.size > 0 && view.size <= 16 * 1024 * 1024 && view.stride == 0,
                "gltf.image_size");
#if defined(RHYTHM_HAS_IMAGE_DECODER)
        const auto bytes = cgltf_buffer_view_data(&view);
        Require(bytes != nullptr, "gltf.image_buffer");
        media::VideoDecoder decoder(std::span<const std::uint8_t>(bytes, view.size), 1, stop);
        auto frame = decoder.Read(stop);
        Require(frame.has_value() && !decoder.Read(stop), "gltf.static_image");
        Require(frame->info_.pixel_aspect_ == 1 && frame->info_.clockwise_rotation_ == 0 &&
                        frame->rgba_.size() <= scene::kMaximumModelImageBytes - total,
                "gltf.image_budget");
        total += frame->rgba_.size();
        // glTF OPAQUE ignores texture alpha, including zero alpha carrying RGB.
        for (std::size_t channel = 3; channel < frame->rgba_.size(); channel += 4)
            frame->rgba_[channel] = 255;
        images.push_back({std::uint16_t(frame->info_.width_), std::uint16_t(frame->info_.height_),
                          std::move(frame->rgba_)});
#else
        throw std::runtime_error("image.decoder_unavailable");
#endif
    }
    return images;
}
void ReadMaterialTextures(const cgltf_data& data, const cgltf_material& source,
                          scene::Material& material, scene::Model& model) {
    auto& slots = material.textures_.images_;
    slots[0] = ImageIndex(data, source.pbr_metallic_roughness.base_color_texture);
    slots[1] = ImageIndex(data, source.normal_texture);
    slots[3] = ImageIndex(data, source.emissive_texture);
    material.textures_.normal_scale_ =
            source.normal_texture.texture ? source.normal_texture.scale : 1;
    Require(std::isfinite(material.textures_.normal_scale_) &&
                    material.textures_.normal_scale_ >= 0 && material.textures_.normal_scale_ <= 4,
            "gltf.normal_scale");
    const auto mr = ImageIndex(data, source.pbr_metallic_roughness.metallic_roughness_texture);
    const auto ao = ImageIndex(data, source.occlusion_texture);
    if (!mr && !ao) return;
    const auto& primary = model.images_.at(mr.value_or(ao.value_or(0)));
    if (mr && ao) {
        const auto& occlusion = model.images_.at(*ao);
        Require(primary.width_ == occlusion.width_ && primary.height_ == occlusion.height_,
                "gltf.orm_extent");
    }
    Require(primary.rgba_.size() <= scene::kMaximumModelImageBytes - ImageBytes(model),
            "gltf.image_budget");
    auto combined = primary;
    const float strength = ao ? source.occlusion_texture.scale : 0;
    Require(std::isfinite(strength) && strength >= 0 && strength <= 1, "gltf.occlusion_strength");
    for (std::size_t offset = 0; offset < combined.rgba_.size(); offset += 4) {
        combined.rgba_[offset] =
                ao ? std::uint8_t(std::lround(255 * (1 - strength) +
                                              model.images_.at(*ao).rgba_[offset] * strength))
                   : 255;
        if (!mr) combined.rgba_[offset + 1] = combined.rgba_[offset + 2] = 255;
    }
    slots[2] = std::uint32_t(model.images_.size());
    model.images_.push_back(std::move(combined));
}
}  // namespace rhythm::model_import::detail
