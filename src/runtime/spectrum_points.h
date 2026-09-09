#pragma once

#include "rhythm/runtime/runtime.h"

namespace rhythm::runtime::detail {
// Canonical log-band interpolation, then canvas-normalized ordered points.
// IDs follow sample order; publication never mutates a previous snapshot.
std::shared_ptr<const particles::PointCloud> SpectrumPoints(const graph::Instruction& instruction,
                                                            std::span<const NodeOutput> outputs,
                                                            const ExternalInputs& external);
}  // namespace rhythm::runtime::detail
