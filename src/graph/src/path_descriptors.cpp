#include "scene_descriptors.h"

namespace rhythm::graph {
void AppendPathDescriptors(std::vector<OperatorDescriptor>& operators) {
    using Type = ValueType;
    operators.push_back({"path.helix",
                         Operation::kPathHelix,
                         Type::kPath,
                         {{"path_radius", Type::kScalar, false},
                          {"path_height", Type::kScalar, false},
                          {"path_turns", Type::kScalar, false},
                          {"path_phase", Type::kScalar, false}},
                         {{"path_samples", 192.0, 3, 1024, {}, true},
                          {"path_radius", 1.2, 0.001, 100},
                          {"path_height", 3.0, -100, 100},
                          {"path_turns", 3.0, 0.01, 32},
                          {"path_phase", 0.0, -36000, 36000},
                          {"path_closed", 0.0, 0, 1, {"option.off", "option.on"}}}});
    operators.push_back({"path.from_points",
                         Operation::kPathFromPoints,
                         Type::kPath,
                         {{"points", Type::kPoints}},
                         {{"path_samples", 256.0, 3, 1024, {}, true},
                          {"path_span", 4.0, 0.01, 100},
                          {"path_closed", 0.0, 0, 1, {"option.off", "option.on"}}}});
    operators.push_back({"path.resample",
                         Operation::kPathResample,
                         Type::kPath,
                         {{"path", Type::kPath}},
                         {{"path_samples", 192.0, 3, 1024, {}, true}}});
    operators.push_back(
            {"geometry.tube",
             Operation::kGeometryTube,
             Type::kGeometry,
             {{"path", Type::kPath}, {"tube_radius", Type::kScalar, false}},
             {{"tube_radius", 0.04, 0.0001, 10}, {"tube_sides", 12.0, 3, 32, {}, true}}});
}
}  // namespace rhythm::graph
