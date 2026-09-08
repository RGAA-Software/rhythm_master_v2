#pragma once

#include <cstdint>
#include <span>

namespace rhythm::render {
class Renderer;
}
namespace rhythm::validation {
// Native test boundary, outside an open Renderer frame. The host owns this
// destination until AFTER device destruction, including on a readback timeout.
void VerifyGpuExecution(std::span<std::uint8_t, 32 * 16 * 4> pixels);
void VerifySceneInstances(render::Renderer& renderer);
}  // namespace rhythm::validation
