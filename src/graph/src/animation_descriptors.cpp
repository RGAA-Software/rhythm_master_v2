#include "scene_descriptors.h"

namespace rhythm::graph {
void AppendAnimationDescriptors(std::vector<OperatorDescriptor>& operators) {
    using Type = ValueType;
    operators.push_back({"geometry.animate",
                         Operation::kGeometryAnimate,
                         Type::kGeometry,
                         {{"geometry", Type::kGeometry},
                          {"time", Type::kScalar, false},
                          {"animation_blend", Type::kScalar, false}},
                         {{"animation_clip", 0.0, 0, 63, {}, true},
                          {"animation_second", 0.0, 0, 63, {}, true},
                          {"animation_blend", 0.0, 0, 1},
                          {"animation_speed", 1.0, -16, 16},
                          {"animation_offset", 0.0, -86400, 86400},
                          {"animation_loop", 1.0, 0, 1, {"option.off", "option.on"}}},
                         true});
}
}  // namespace rhythm::graph
