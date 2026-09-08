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
void VerifySampleableDepth(render::Renderer& renderer);
void VerifyPositionalLights(render::Renderer& renderer);
void VerifyMaterialTextures(render::Renderer& renderer);
void VerifyModelImages(render::Renderer& renderer);
void VerifyEnvironmentLighting(render::Renderer& renderer);
void VerifySceneShadows(render::Renderer& renderer);
void VerifyColorPipeline(render::Renderer& renderer);
void VerifyGpuParticles(render::Renderer& renderer);
void VerifySceneInstances(render::Renderer& renderer);
}  // namespace rhythm::validation
