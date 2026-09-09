#pragma once

#include <filesystem>

#include "rhythm/render/renderer.h"

namespace rhythm::validation {
void VerifySceneCompositor(render::Renderer& renderer);
void VerifySceneReplacement(render::Renderer& renderer, const std::filesystem::path& root);
void VerifySceneDeck(render::Renderer& renderer, const std::filesystem::path& first,
                     const std::filesystem::path& second);
}  // namespace rhythm::validation
