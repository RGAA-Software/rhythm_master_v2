#pragma once

#include "rhythm/graph/registry.h"
#include "rhythm/render/renderer.h"

namespace rhythm::runtime::detail {
void DrawShape(const graph::Node& node, render::TextureHandle white, render::DrawList& list);
}  // namespace rhythm::runtime::detail
