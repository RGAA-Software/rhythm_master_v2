#pragma once

#include <filesystem>

#include "rhythm/render/renderer.h"

namespace rhythm::validation {
void VerifySceneCompositor(render::Renderer& renderer);
void VerifySceneDeck(render::Renderer& renderer, const std::filesystem::path& first,
                     const std::filesystem::path& second);
}  // namespace rhythm::validation
