#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "rhythm/editor/transform_drivers.h"

namespace {
using namespace rhythm;
void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
void Run() {
    graph::Registry registry;
    editor::Snapshot snapshot;
    auto& document = snapshot.document_;
    document.id_ = "transform-driver-contract";
    document.revision_ = 42;
    document.nodes_ = {
            registry.MakeNode(1, "texture.gradient"), registry.MakeNode(2, "texture.affine"),
            registry.MakeNode(3, "output.texture"),   registry.MakeNode(4, "scalar.curve"),
            registry.MakeNode(5, "core.time"),        registry.MakeNode(6, "scalar.constant"),
            registry.MakeNode(7, "texture.affine")};
    document.edges_ = {{1, 1, 2, "source"},      {2, 2, 3, "source"}, {3, 5, 4, "time"},
                       {4, 4, 2, "translate_x"}, {5, 6, 2, "scale"},  {6, 1, 7, "source"},
                       {7, 6, 7, "scale"}};
    document.signals_ = {{"motion", 4}};
    document.bindings_ = {{2, "translate_y", "motion"}, {7, "rotation", "motion"}};
    document.output_ = 3;
    document.nodes_[3].properties_["curve"] =
            parameters::Curve({{0, 0, parameters::Interpolation::kHermite, .1, .2},
                               {1, 1, parameters::Interpolation::kHermite, .3, .4}});
    editor::TransformSample sample{document.id_, 42, {{4, .4}, {5, .25}, {6, 10}}};
    const auto drivers = std::get<std::vector<editor::TransformDriver>>(
            editor::InspectTransformDrivers(document, registry, 2));
    Check(drivers.size() == 3 && drivers[0].property_ == "scale" &&
                  drivers[1].source_type_ == "scalar.curve" && drivers[1].curve_clock_ == 5 &&
                  drivers[2].signal_ == "motion",
          "driver types, named origin and actual curve clock");
    const auto original = snapshot;
    const auto frozen = std::get<editor::Snapshot>(
            editor::FreezeTransformDrivers(snapshot, registry, 2, sample));
    Check(graph::Scalar(frozen.document_.nodes_[1], "scale", 0) == 8 &&
                  graph::Scalar(frozen.document_.nodes_[1], "translate_x", 0) == .4 &&
                  graph::Scalar(frozen.document_.nodes_[1], "translate_y", 0) == .4,
          "freeze retains observed clamped values");
    Check(frozen.document_.edges_.size() == 5 && frozen.document_.bindings_.size() == 1 &&
                  frozen.document_.bindings_[0].node_ == 7 &&
                  frozen.document_.signals_ == document.signals_ &&
                  frozen.document_.nodes_.size() == document.nodes_.size() && snapshot == original,
          "freeze only detaches selected transform, preserving producers and other consumers");
    Check(std::holds_alternative<graph::ExecutionPlan>(graph::Compile(frozen.document_, registry)),
          "frozen author graph remains publishable");
    editor::History history(snapshot);
    Check(history.Apply(frozen, 42) && history.Undo() && !history.Undo() &&
                  history.Current().document_.edges_ == document.edges_ &&
                  history.Current().document_.bindings_ == document.bindings_,
          "one undo restores drivers");
    const auto recorded = std::get<editor::Snapshot>(
            editor::RecordTransformKey(snapshot, registry, 2, "translate_y", .75, sample));
    const auto& curve =
            std::get<parameters::Curve>(recorded.document_.nodes_[3].properties_.at("curve"));
    Check(curve.Keys().size() == 3 && curve.Keys()[1].seconds_ == .25 &&
                  curve.Keys()[1].value_ == .75 && curve.Keys()[0].out_slope_ == .2 &&
                  curve.Keys()[2].in_slope_ == .3,
          "record at observed local input time; preserve other keys and tangents");
    Check(recorded.document_.edges_ == document.edges_ &&
                  recorded.document_.bindings_ == document.bindings_ &&
                  recorded.document_.nodes_[1] == document.nodes_[1],
          "keyframe edits explicit shared curve without disconnecting automation");
    sample.scalars_[5] = 1;
    const auto replaced = std::get<editor::Snapshot>(
            editor::RecordTransformKey(snapshot, registry, 2, "translate_x", -.2, sample));
    const auto& replaced_curve =
            std::get<parameters::Curve>(replaced.document_.nodes_[3].properties_.at("curve"));
    Check(replaced_curve.Keys().size() == 2 && replaced_curve.Keys()[1].value_ == -.2 &&
                  replaced_curve.Keys()[1].out_slope_ == .4,
          "replace exact key without duplicates or tangent loss");
    Check(std::holds_alternative<graph::Diagnostic>(
                  editor::RecordTransformKey(snapshot, registry, 2, "scale", 2, sample)),
          "non-curve driver cannot receive invented keys");
    sample.revision_ = 41;
    Check(std::get<graph::Diagnostic>(editor::FreezeTransformDrivers(snapshot, registry, 2, sample))
                          .code_ == "transform_driver.stale_frame",
          "stale frame cannot overwrite drivers");
    sample.revision_ = 42;
    sample.scalars_[6] = std::numeric_limits<double>::quiet_NaN();
    Check(std::get<graph::Diagnostic>(editor::FreezeTransformDrivers(snapshot, registry, 2, sample))
                                  .code_ == "transform_driver.missing_value" &&
                  snapshot == original,
          "invalid observation rejects atomically");
    sample.scalars_[6] = 10;
    sample.scalars_[5] = -1;
    Check(std::get<graph::Diagnostic>(
                  editor::RecordTransformKey(snapshot, registry, 2, "translate_x", .2, sample))
                          .code_ == "transform_driver.curve_time",
          "negative local clock is not a valid authored key time");
    sample.scalars_[5] = .25;
    std::vector<parameters::Keyframe> keys;
    for (std::size_t index = 0; index < parameters::Curve::kMaximumKeys; ++index)
        keys.push_back({double(index), 0});
    snapshot.document_.nodes_[3].properties_["curve"] = parameters::Curve(keys);
    Check(std::holds_alternative<graph::Diagnostic>(
                  editor::RecordTransformKey(snapshot, registry, 2, "translate_x", .2, sample)),
          "curve key budget preserved");
    snapshot = original;
    snapshot.document_.nodes_[1] = registry.MakeNode(2, "scene.transform");
    const auto scene_frozen = std::get<editor::Snapshot>(
            editor::FreezeTransformDrivers(snapshot, registry, 2, sample));
    Check(graph::Scalar(scene_frozen.document_.nodes_[1], "scale", 0) == 10,
          "3D bounds are read from its own descriptor, not 2D limits");
    Check(std::holds_alternative<graph::Diagnostic>(
                  editor::InspectTransformDrivers(document, registry, 1)),
          "ordinary image nodes cannot be treated as transform authors");
}
}  // namespace
int main() {
    try {
        Run();
        std::cout << "Transform automation: scoped freeze, shared curve keys, local clocks, undo "
                     "and stale/budget guards passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
