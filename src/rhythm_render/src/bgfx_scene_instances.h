#pragma once

#include <span>

#include "bgfx_handles.h"
#include "rhythm/render/scene.h"

namespace rhythm::render::detail {
bool Mirrored(const Matrix4& matrix);

// Device-thread owner of the instance program and transient packing contract.
// Consecutive compatible opaque records retain ordering and normal transforms.
class BgfxSceneInstances final {
   public:
    BgfxSceneInstances();
    // Returns 1 without binding when unsupported, transparent, or out of space.
    std::uint32_t Bind(std::span<const MeshDraw> draws) const;
    bgfx::ProgramHandle Program() const { return program_.Get(); }

   private:
    GpuHandle<bgfx::ProgramHandle> program_{};
};
}  // namespace rhythm::render::detail
