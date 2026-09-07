#include "bgfx_readbacks.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace rhythm::render::detail {
BgfxReadbacks::BgfxReadbacks(std::shared_ptr<ReadbackMemory> memory) : memory_(std::move(memory)) {}
bool BgfxReadbacks::Supported() {
    const auto* caps = bgfx::getCaps();
    constexpr auto kRequired = BGFX_CAPS_TEXTURE_BLIT | BGFX_CAPS_TEXTURE_READ_BACK;
    return caps && (caps->supported & kRequired) == kRequired &&
           bgfx::isTextureValid(0, false, 1, bgfx::TextureFormat::RGBA8,
                                BGFX_TEXTURE_READ_BACK | BGFX_TEXTURE_BLIT_DST);
}
std::uint64_t BgfxReadbacks::Request(bgfx::TextureHandle source, Extent extent, bgfx::ViewId view,
                                     ResourceTable& resources) {
    const auto count = static_cast<std::size_t>(extent.width_) * extent.height_;
    if (!count || count > 2073600) throw std::invalid_argument("render.readback_extent");
    if (next_ticket_ == std::numeric_limits<std::uint64_t>::max())
        throw std::overflow_error("render.readback_generation");
    const auto found = std::find_if(slots_.begin(), slots_.end(),
                                    [](const Slot& slot) { return slot.stage_ == Stage::kEmpty; });
    if (found == slots_.end()) throw std::runtime_error("render.readback_queue_full");
    const auto index = static_cast<std::size_t>(found - slots_.begin());
    auto& image = memory_->images_[index];
    auto& slot = *found;
    try {
        image.extent_ = extent;
        image.rgba_.resize(count * 4);
        // The nonempty RGBA span reserves an uploaded-image slot (no framebuffer
        // or depth). Actual allocation is a private native staging texture.
        slot.budget_ = resources.Allocate(extent, image.rgba_, TexturePrecision::kUnorm8);
        slot.texture_ = GpuHandle(bgfx::createTexture2D(
                extent.width_, extent.height_, false, 1, bgfx::TextureFormat::RGBA8,
                BGFX_TEXTURE_READ_BACK | BGFX_TEXTURE_BLIT_DST));
        bgfx::TextureRegion from;
        from.init(source);
        bgfx::TextureRegion to;
        to.init(slot.texture_.Get());
        bgfx::blit(view, to, from);
        slot.stage_ = Stage::kBlit;
        slot.ticket_ = next_ticket_++;
        return slot.ticket_;
    } catch (...) {
        Release(index, resources);
        throw;
    }
}
void BgfxReadbacks::Release(std::size_t index, ResourceTable& resources) {
    resources.Release(slots_[index].budget_);
    slots_[index] = {};
    memory_->images_[index] = {};
}
void BgfxReadbacks::Advance(std::uint32_t completed, ResourceTable& resources) {
    for (std::size_t index = 0; index < slots_.size(); ++index) {
        auto& slot = slots_[index];
        if (slot.stage_ == Stage::kRead &&
            static_cast<std::int32_t>(completed - slot.ready_) >= 0) {
            slot.stage_ = Stage::kReady;
            if (slot.canceled_) Release(index, resources);
        } else if (slot.stage_ == Stage::kBlit) {
            if (slot.canceled_) {
                Release(index, resources);
            } else {
                // The blit was submitted by this EndFrame. Issue read after it,
                // retaining owned storage through the returned completion frame.
                bgfx::TextureRegion source;
                source.init(slot.texture_.Get());
                slot.ready_ = bgfx::read(source, memory_->images_[index].rgba_.data());
                slot.stage_ = Stage::kRead;
            }
        }
    }
}
std::optional<ReadbackImage> BgfxReadbacks::Poll(std::uint64_t ticket, ResourceTable& resources) {
    for (std::size_t index = 0; index < slots_.size(); ++index) {
        auto& slot = slots_[index];
        if (!ticket || slot.ticket_ != ticket) continue;
        if (slot.canceled_) throw std::logic_error("render.readback_canceled");
        if (slot.stage_ != Stage::kReady) return std::nullopt;
        auto result = std::move(memory_->images_[index]);
        Release(index, resources);
        return result;
    }
    throw std::logic_error("render.readback_stale");
}
void BgfxReadbacks::Cancel(std::uint64_t ticket, ResourceTable& resources) noexcept {
    resources.CheckThread();
    for (std::size_t index = 0; index < slots_.size(); ++index) {
        auto& slot = slots_[index];
        if (!ticket || slot.ticket_ != ticket) continue;
        slot.canceled_ = true;
        if (slot.stage_ == Stage::kReady) Release(index, resources);
        return;
    }
}
void BgfxReadbacks::Invalidate(ResourceTable& resources) {
    for (auto& slot : slots_) Cancel(slot.ticket_, resources);
}
}  // namespace rhythm::render::detail
