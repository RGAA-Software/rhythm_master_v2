#pragma once
#include "rhythm/runtime/runtime.h"

namespace rhythm::runtime::detail {
double EvaluateScalar(const graph::Instruction& instruction, std::span<const NodeOutput> outputs,
                      double seconds);
std::optional<double> ExternalScalar(const graph::Instruction& instruction,
                                     const FrameContext& frame);
}  // namespace rhythm::runtime::detail
