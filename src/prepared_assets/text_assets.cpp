#include "text_assets.h"

#include <algorithm>
#include <map>
#include <stdexcept>

#include "rhythm/graph/text.h"
#include "rhythm/text/rasterizer.h"

namespace rhythm::prepared_assets::detail {
void PrepareText(const graph::ExecutionPlan& plan, std::span<const project::PackagedAsset> assets,
                 assets::Images& images, std::size_t model_image_bytes, std::stop_token stop) {
    std::map<std::string, std::unique_ptr<text::Rasterizer>> fonts;
    std::size_t font_bytes = 0;
    std::size_t image_bytes = model_image_bytes;
    std::size_t layouts = 0;
    for (const auto& image : images.images_) image_bytes += image.rgba_.size();
    for (const auto& instruction : plan.instructions_) {
        if (stop.stop_requested()) throw std::runtime_error("asset.cancelled");
        if (instruction.operation_ != graph::Operation::kTextureText) continue;
        const auto& node = instruction.node_;
        const auto& id = std::get<assets::AssetId>(node.properties_.at("asset"));
        const auto key = graph::TextImageKey(node);
        if (std::any_of(images.images_.begin(), images.images_.end(), [&](const auto& image) {
                return image.id_ == id && image.variant_key_ == key;
            }))
            continue;
        if (++layouts > 64) throw std::length_error("text.layout_count");
        auto& font = fonts[id.sha256_];
        if (!font) {
            const auto found = std::find_if(assets.begin(), assets.end(), [&](const auto& asset) {
                return asset.record_.id_ == id;
            });
            if (found == assets.end()) throw std::invalid_argument("text.font_missing");
            if (found->record_.media_type_ != "font/otf" &&
                found->record_.media_type_ != "font/ttf")
                throw std::invalid_argument("text.font_media_type");
            if (found->bytes_.size() > 64 * 1024 * 1024 - font_bytes)
                throw std::length_error("text.font_budget");
            font_bytes += found->bytes_.size();
            // The package owns these bytes during this synchronous call;
            // Rasterizer copies them into its private font lifetime.
            font = std::make_unique<text::Rasterizer>(std::span<const std::uint8_t>(
                    reinterpret_cast<const std::uint8_t*>(found->bytes_.data()),
                    found->bytes_.size()));
        }
        text::Layout layout;
        layout.text_ = std::get<std::string>(node.properties_.at("text_content"));
        layout.width_ = static_cast<std::uint32_t>(graph::Scalar(node, "text_width", 512));
        layout.height_ = static_cast<std::uint32_t>(graph::Scalar(node, "text_height", 256));
        layout.pixel_size_ = static_cast<std::uint32_t>(graph::Scalar(node, "text_size", 48));
        layout.line_spacing_ = static_cast<float>(graph::Scalar(node, "text_spacing", 1.2));
        layout.alignment_ = static_cast<text::Alignment>(
                static_cast<int>(graph::Scalar(node, "text_align", 0)));
        layout.wrap_ = graph::Scalar(node, "text_wrap", 1) != 0;
        const auto bytes = std::size_t(layout.width_) * layout.height_ * 4;
        if (image_bytes > assets::kMaximumImageBytes ||
            bytes > assets::kMaximumImageBytes - image_bytes)
            throw std::length_error("image.byte_budget");
        const auto mask = font->Render(layout);
        assets::ImageResource image;
        image.id_ = id;
        image.width_ = static_cast<std::uint16_t>(mask.width_);
        image.height_ = static_cast<std::uint16_t>(mask.height_);
        image.variant_key_ = key;
        image.missing_glyphs_ = mask.missing_glyphs_;
        image.clipped_ = mask.clipped_;
        image.rgba_.resize(bytes, 255);
        for (std::size_t index = 0; index < mask.coverage_.size(); ++index)
            image.rgba_[index * 4 + 3] = mask.coverage_[index];
        images.images_.push_back(std::move(image));
        image_bytes += bytes;
    }
}
}  // namespace rhythm::prepared_assets::detail
