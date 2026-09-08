#include "color_descriptors.h"
namespace rhythm::graph {
void AppendColorDescriptors(std::vector<OperatorDescriptor>& operators) {
    using Type = ValueType;
    operators.push_back({"texture.fxaa",
                         Operation::kTextureFxaa,
                         Type::kTexture,
                         {{"source", Type::kTexture}, {"fxaa_strength", Type::kScalar, false}},
                         {{"fxaa_strength", 1.0, 0, 1},
                          {"fxaa_span", 8.0, 1, 16},
                          {"fxaa_reduce_multiplier", 0.125, 0.01, 1},
                          {"fxaa_reduce_minimum", 0.0078125, 0.001, 0.25}}});
    operators.push_back({"texture.linearize",
                         Operation::kTextureLinearize,
                         Type::kTexture,
                         {{"source", Type::kTexture}}});
    operators.push_back({"texture.display",
                         Operation::kTextureDisplay,
                         Type::kTexture,
                         {{"source", Type::kTexture}, {"exposure", Type::kScalar, false}},
                         {{"exposure", 0.0, -8, 8},
                          {"tone_mapping", 1.0, 0, 1, {"tone.clip", "tone.reinhard"}}}});
    // New nodes may explicitly inherit precision. Legacy nodes without this
    // property retain RGBA8; no hidden change to their existing render path.
    for (auto& descriptor : operators) {
        if ((descriptor.output_ != Type::kTexture && descriptor.output_ != Type::kSceneImage) ||
            descriptor.operation_ == Operation::kOutput ||
            descriptor.operation_ == Operation::kSceneColor ||
            descriptor.operation_ == Operation::kTextureTrail)
            continue;
        const double initial =
                descriptor.operation_ == Operation::kTextureLinearize ||
                                descriptor.operation_ == Operation::kDepthLinearize ||
                                descriptor.operation_ == Operation::kSceneCapture
                        ? 2
                : descriptor.operation_ == Operation::kTextureDisplay ||
                                descriptor.operation_ == Operation::kFeedback
                        ? 1
                        : 0;
        descriptor.properties_.push_back(
                {"texture_precision",
                 initial,
                 0,
                 2,
                 {"precision.inherit", "precision.unorm8", "precision.float16"}});
    }
}
}  // namespace rhythm::graph
