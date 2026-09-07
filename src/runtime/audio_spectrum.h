#pragma once

#include "rhythm/graph/document.h"
#include "rhythm/render/renderer.h"
#include "rhythm/runtime/inputs.h"

namespace rhythm::runtime::detail {
std::span<const float> SpectrumBands(const graph::Node& node, const ExternalInputs& inputs);
// Builds all bars into one draw command; resamples canonical log bands once.
void DrawSpectrum(const graph::Node& node, std::span<const float> bands,
                  render::TextureHandle white, render::DrawList& list);
}  // namespace rhythm::runtime::detail
