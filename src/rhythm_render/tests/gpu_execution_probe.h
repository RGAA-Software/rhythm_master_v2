#pragma once

#include <cstdint>
#include <filesystem>
#include <span>

namespace rhythm::render {
class Renderer;
}
namespace rhythm::validation {
// Native test boundary, outside an open Renderer frame. The host owns this
// destination until AFTER device destruction, including on a readback timeout.
void VerifyGpuExecution(std::span<std::uint8_t, 32 * 16 * 4> pixels);
void VerifyMaterialProfile(std::span<std::uint8_t, 32 * 16 * 4> pixels,
                           const std::filesystem::path& directory);
void VerifySampleableDepth(render::Renderer& renderer);
void VerifyPositionalLights(render::Renderer& renderer);
void VerifyMaterialTextures(render::Renderer& renderer);
void VerifyModelImages(render::Renderer& renderer);
void VerifyEnvironmentLighting(render::Renderer& renderer);
void VerifyMeshDeformation(render::Renderer& renderer);
void VerifyMeshSkinning(render::Renderer& renderer);
void VerifyMeshMorph(render::Renderer& renderer);
void VerifyModelSkin(render::Renderer& renderer);
void VerifyModelMorph(render::Renderer& renderer);
void VerifyImageProgram(render::Renderer& renderer, const std::filesystem::path& path);
void VerifySceneShadows(render::Renderer& renderer);
void VerifyColorPipeline(render::Renderer& renderer);
void VerifyFxaa(render::Renderer& renderer);
void VerifyGpuParticles(render::Renderer& renderer);
void VerifySceneInstances(render::Renderer& renderer);
void MeasureQualityBaseline(render::Renderer& renderer, const std::filesystem::path& directory);
}  // namespace rhythm::validation
