#include "scene_descriptors.h"

namespace rhythm::graph {
void AppendPathDescriptors(std::vector<OperatorDescriptor>& operators) {
    using Type = ValueType;
    operators.push_back({"texture.path_fill",
                         Operation::kVectorFill,
                         Type::kTexture,
                         {{"path", Type::kPath}, {"hole", Type::kPath, false}},
                         {{"path_samples", 512.0, 3, 512, {}, true},
                          {"path_span", 4.0, 0.1, 100},
                          {"path_plane", 0.0, 0, 2, {"vector.xy", "vector.xz", "vector.yz"}, true},
                          {"color_a", Color{0.1, 0.8, 0.9, 1}}}});
    operators.push_back(
            {"texture.path_stroke",
             Operation::kVectorStroke,
             Type::kTexture,
             {{"path", Type::kPath}, {"vector_width", Type::kScalar, false}},
             {{"path_samples", 512.0, 3, 512, {}, true},
              {"path_span", 4.0, 0.1, 100},
              {"path_plane", 0.0, 0, 2, {"vector.xy", "vector.xz", "vector.yz"}, true},
              {"vector_width", 4.0, 0, 100},
              {"vector_join", 2.0, 0, 2, {"vector.miter", "vector.bevel", "vector.round"}, true},
              {"vector_cap", 2.0, 0, 2, {"vector.butt", "vector.square", "vector.round"}, true},
              {"vector_miter", 4.0, 1, 16},
              {"vector_tolerance", 0.25, 0.001, 1},
              {"color_a", Color{0.1, 0.8, 0.9, 1}}}});
    operators.push_back({"geometry.deform",
                         Operation::kGeometryDeform,
                         Type::kGeometry,
                         {{"geometry", Type::kGeometry},
                          {"deform_twist", Type::kScalar, false},
                          {"deform_taper", Type::kScalar, false}},
                         {{"deform_twist", 45.0, -720, 720},
                          {"deform_taper", 0.0, -4, 4},
                          {"deform_axis", 1.0, 0, 2, {"axis.x", "axis.y", "axis.z"}},
                          {"deform_pivot_x", 0.0, -10000, 10000},
                          {"deform_pivot_y", 0.0, -10000, 10000},
                          {"deform_pivot_z", 0.0, -10000, 10000}}});
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
