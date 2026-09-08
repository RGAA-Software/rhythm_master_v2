#include "depth_descriptors.h"
namespace rhythm::graph {
void AppendDepthDescriptors(std::vector<OperatorDescriptor>& operators) {
    using Type = ValueType;
    operators.push_back({"scene.capture",
                         Operation::kSceneCapture,
                         Type::kSceneImage,
                         {{"scene", Type::kScene}, {"camera", Type::kCamera, false}}});
    operators.push_back({"scene.color",
                         Operation::kSceneColor,
                         Type::kTexture,
                         {{"capture", Type::kSceneImage}}});
    operators.push_back({"scene.depth",
                         Operation::kSceneDepth,
                         Type::kDepth,
                         {{"capture", Type::kSceneImage}}});
    operators.push_back({"depth.linearize",
                         Operation::kDepthLinearize,
                         Type::kTexture,
                         {{"depth", Type::kDepth}},
                         {{"depth_normalize", 1.0, 0, 1, {"depth.distance", "depth.normalized"}}}});
    operators.push_back({"texture.dof",
                         Operation::kDepthOfField,
                         Type::kTexture,
                         {{"source", Type::kTexture},
                          {"depth", Type::kDepth},
                          {"focus_distance", Type::kScalar, false},
                          {"focus_scale", Type::kScalar, false}},
                         {{"focus_distance", 3.0, 0.001, 100000},
                          {"focus_scale", 4.0, 0, 1000},
                          {"dof_radius", 12.0, 0, 32},
                          {"dof_samples", 32.0, 1, 64, {}, true}}});
}
}  // namespace rhythm::graph
