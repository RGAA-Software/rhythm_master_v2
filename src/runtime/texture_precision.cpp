#include "texture_ops.h"
namespace rhythm::runtime::detail {
render::TexturePrecision OutputPrecision(const graph::Instruction& instruction,
                                         std::span<const NodeOutput> outputs,
                                         const render::Renderer& renderer) {
    using Precision = render::TexturePrecision;
    const auto fallback =
            instruction.operation_ == graph::Operation::kTextureLinearize ||
                            instruction.operation_ == graph::Operation::kDepthLinearize ||
                            instruction.operation_ == graph::Operation::kSceneCapture ||
                            instruction.operation_ == graph::Operation::kTextureGlow
                    ? 2
                    : 1;
    const auto mode = graph::Scalar(instruction.node_, "texture_precision", fallback);
    if (mode == 2) return Precision::kFloat16;
    if (mode != 0) return Precision::kUnorm8;
    // Feedback crosses a future edge; its history precision must be explicit.
    if (instruction.operation_ == graph::Operation::kFeedback) return Precision::kUnorm8;
    for (const auto source : instruction.inputs_) {
        if (!source) continue;
        const auto handle = outputs[*source].texture_;
        if (renderer.IsValid(handle) && renderer.Precision(handle) == Precision::kFloat16)
            return Precision::kFloat16;
    }
    return Precision::kUnorm8;
}
}  // namespace rhythm::runtime::detail
