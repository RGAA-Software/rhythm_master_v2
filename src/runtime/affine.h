#pragma once

#include "rhythm/runtime/runtime.h"

namespace rhythm::runtime::detail {
// Canvas-relative translation and pivot; rotation is clockwise in degrees.
// Connected scalar values override properties and are clamped to their bounds.
void DrawAffine(const graph::Instruction& instruction, std::span<const NodeOutput> outputs,
                render::DrawList& list);
}  // namespace rhythm::runtime::detail
