#include "rhythm/prepared_assets/prepare.h"

#include <algorithm>
#include <set>
#include <stdexcept>

#include "rhythm/graph/text.h"
#include "shader_assets.h"
#include "text_assets.h"
#include "video_assets.h"

#if defined(RHYTHM_HAS_IMAGE_DECODER)
#include "rhythm/media/video_decoder.h"
#endif

namespace rhythm::prepared_assets {
bool Covers(const graph::ExecutionPlan& plan, const Resources& resources) {
    if (!resources.models_ || !resources.images_ || !resources.shaders_ ||
        !model_assets::Covers(plan, *resources.models_))
        return false;
    std::size_t video_instances = 0;
    for (const auto& instruction : plan.instructions_) {
        if (instruction.operation_ == graph::Operation::kTextureShader) {
            const auto& id = std::get<assets::AssetId>(instruction.node_.properties_.at("asset"));
            if (!resources.shaders_->programs_.contains(id.sha256_)) return false;
        }
        if (instruction.operation_ == graph::Operation::kTextureVideo) {
            if (++video_instances > 4) return false;
            const auto& id = std::get<assets::AssetId>(instruction.node_.properties_.at("asset"));
            if (std::none_of(resources.videos_.begin(), resources.videos_.end(),
                             [&](const auto& video) { return video.id_ == id; }))
                return false;
        }
        if (instruction.operation_ != graph::Operation::kTextureImage &&
            instruction.operation_ != graph::Operation::kTextureText)
            continue;
        const auto& id = std::get<assets::AssetId>(instruction.node_.properties_.at("asset"));
        const auto key = instruction.operation_ == graph::Operation::kTextureText
                                 ? graph::TextImageKey(instruction.node_)
                                 : std::string{};
        if (std::none_of(resources.images_->images_.begin(), resources.images_->images_.end(),
                         [&](const auto& image) {
                             return image.id_ == id && image.variant_key_ == key;
                         }))
            return false;
    }
    return true;
}

std::shared_ptr<const Resources> Prepare(const graph::ExecutionPlan& plan,
                                         std::span<const project::PackagedAsset> assets,
                                         std::stop_token stop) {
    auto result = std::make_shared<Resources>();
    // Existing model preparation also verifies every record, hash and byte budget.
    result->models_ = model_assets::Prepare(plan, assets, stop);
    auto images = std::make_shared<assets::Images>();
    std::set<std::string> ids;
#if defined(RHYTHM_HAS_IMAGE_DECODER)
    std::size_t total = 0;
    for (const auto& model : result->models_->models_) {
        if (model.image_bytes_ > assets::kMaximumImageBytes - total)
            throw std::length_error("image.byte_budget");
        total += model.image_bytes_;
    }
#endif
    for (const auto& instruction : plan.instructions_) {
        if (stop.stop_requested()) throw std::runtime_error("asset.cancelled");
        if (instruction.operation_ != graph::Operation::kTextureImage) continue;
        const auto& id = std::get<assets::AssetId>(instruction.node_.properties_.at("asset"));
        if (!assets::ValidId(id)) throw std::invalid_argument("image.asset_missing");
        if (!ids.insert(id.sha256_).second) continue;
        const auto found = std::find_if(assets.begin(), assets.end(),
                                        [&](const auto& asset) { return asset.record_.id_ == id; });
        if (found == assets.end()) throw std::invalid_argument("image.asset_missing");
        const auto& mime = found->record_.media_type_;
        if (mime != "image/png" && mime != "image/jpeg" && mime != "image/webp" &&
            mime != "image/bmp")
            throw std::invalid_argument("image.media_type");
#if defined(RHYTHM_HAS_IMAGE_DECODER)
        const auto bytes = std::span<const std::uint8_t>(
                reinterpret_cast<const std::uint8_t*>(found->bytes_.data()), found->bytes_.size());
        media::VideoDecoder decoder(bytes, 1, stop);
        auto frame = decoder.Read(stop);
        if (!frame || decoder.Read(stop)) throw std::invalid_argument("image.static_required");
        if (frame->rgba_.size() > assets::kMaximumImageBytes - total)
            throw std::length_error("image.byte_budget");
        total += frame->rgba_.size();
        images->images_.push_back({id, static_cast<std::uint16_t>(frame->info_.width_),
                                   static_cast<std::uint16_t>(frame->info_.height_),
                                   frame->info_.pixel_aspect_, frame->info_.clockwise_rotation_,
                                   std::move(frame->rgba_)});
#else
        throw std::runtime_error("image.decoder_unavailable");
#endif
    }
    if (stop.stop_requested()) throw std::runtime_error("asset.cancelled");
    std::size_t model_image_bytes = 0;
    for (const auto& model : result->models_->models_) model_image_bytes += model.image_bytes_;
    detail::PrepareText(plan, assets, *images, model_image_bytes, stop);
    result->images_ = std::move(images);
    result->videos_ = detail::PrepareVideos(plan, assets, stop);
    result->shaders_ = detail::PrepareShaders(plan, assets, stop);
    return result;
}
}  // namespace rhythm::prepared_assets
