#include "image_pass.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <set>
#include <stdexcept>

#include "rhythm/graph/text.h"
#include "texture_ops.h"

namespace rhythm::runtime::detail {
namespace {
const assets::ImageResource& Find(const assets::Images& images, const assets::AssetId& id,
                                  const std::string& variant) {
    const auto found = std::find_if(
            images.images_.begin(), images.images_.end(),
            [&](const auto& image) { return image.id_ == id && image.variant_key_ == variant; });
    if (found == images.images_.end()) throw std::invalid_argument("image.asset_missing");
    return *found;
}
}  // namespace
void ImageUploads::Retain(const graph::ExecutionPlan& plan, const assets::Images& images) {
    std::set<std::string> required;
    std::size_t bytes = 0;
    for (const auto& instruction : plan.instructions_) {
        if (instruction.operation_ != graph::Operation::kTextureImage &&
            instruction.operation_ != graph::Operation::kTextureText)
            continue;
        const auto& id = std::get<assets::AssetId>(instruction.node_.properties_.at("asset"));
        if (!assets::ValidId(id)) throw std::invalid_argument("image.asset_missing");
        const auto variant = instruction.operation_ == graph::Operation::kTextureText
                                     ? graph::TextImageKey(instruction.node_)
                                     : std::string{};
        if (!required.insert(id.sha256_ + variant).second) continue;
        const auto& image = Find(images, id, variant);
        if (!image.width_ || !image.height_ || image.width_ > 4096 || image.height_ > 4096 ||
            std::uint64_t(image.width_) * image.height_ > 2073600 ||
            image.rgba_.size() != std::size_t(image.width_) * image.height_ * 4 ||
            !std::isfinite(image.pixel_aspect_) || image.pixel_aspect_ <= 0 ||
            image.pixel_aspect_ > 100 || !std::isfinite(image.clockwise_rotation_))
            throw std::invalid_argument("image.invalid");
        if (image.rgba_.size() > assets::kMaximumImageBytes - bytes)
            throw std::length_error("image.memory_budget");
        bytes += image.rgba_.size();
    }
    std::erase_if(textures_, [&](const auto& item) { return !required.contains(item.first); });
}
render::DrawList ImageUploads::Draw(const graph::Node& node, render::Extent extent,
                                    const assets::Images& images, render::Renderer& renderer) {
    const auto& id = std::get<assets::AssetId>(node.properties_.at("asset"));
    const auto variant = node.type_ == "texture.text" ? graph::TextImageKey(node) : std::string{};
    const auto& image = Find(images, id, variant);
    auto& texture = textures_[id.sha256_ + variant];
    if (!renderer.IsValid(texture.Handle()))
        texture = renderer.CreateTexture({image.width_, image.height_}, image.rgba_);
    return FramedImageDraw(texture.Handle(), extent,
                           {{image.width_, image.height_},
                            image.pixel_aspect_,
                            image.clockwise_rotation_,
                            graph::Scalar(node, "image_fill", 0) != 0});
}
render::DrawList FramedImageDraw(render::TextureHandle texture, render::Extent extent,
                                 const ImagePlacement& placement) {
    render::DrawList draw;
    draw.width_ = extent.width_;
    draw.height_ = extent.height_;
    AppendTextureQuad(draw, texture, 0xffffffff, 0xffffffff);
    const auto angle = placement.clockwise_rotation_ * std::numbers::pi / 180;
    const double cosine = std::cos(angle), sine = std::sin(angle);
    const double width = placement.source_.width_ * placement.pixel_aspect_,
                 height = placement.source_.height_;
    const double bounding_width = std::abs(cosine) * width + std::abs(sine) * height;
    const double bounding_height = std::abs(sine) * width + std::abs(cosine) * height;
    const double scale_x = extent.width_ / bounding_width,
                 scale_y = extent.height_ / bounding_height;
    const auto scale = !placement.fill_ ? std::min(scale_x, scale_y) : std::max(scale_x, scale_y);
    for (auto& vertex : draw.vertices_) {
        const double x = (vertex.u_ - 0.5) * width, y = (vertex.v_ - 0.5) * height;
        vertex.x_ = static_cast<float>(extent.width_ * 0.5 + scale * (cosine * x - sine * y));
        vertex.y_ = static_cast<float>(extent.height_ * 0.5 + scale * (sine * x + cosine * y));
    }
    return draw;
}
}  // namespace rhythm::runtime::detail
