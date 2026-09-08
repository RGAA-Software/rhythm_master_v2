#pragma once

#include "rhythm/runtime/runtime.h"

namespace rhythm::runtime::detail {
render::TexturePrecision OutputPrecision(const graph::Instruction& instruction,
                                         std::span<const NodeOutput> outputs,
                                         const render::Renderer& renderer);
void AppendTextureQuad(render::DrawList& list, render::TextureHandle source, std::uint32_t top,
                       std::uint32_t bottom, double scale = 1);
// Builds owned drawing commands and returns the texture clear color. Does not
// allocate GPU resources or mutate runtime caches.
std::uint32_t DrawTexture(const graph::Instruction& instruction,
                          std::span<const NodeOutput> outputs, const ExternalInputs& external,
                          render::TextureHandle white, render::DrawList& list);
}  // namespace rhythm::runtime::detail
