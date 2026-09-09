#include <cmath>
#include <iostream>
#include <stdexcept>

#include "rhythm/editor/scene_edit.h"

namespace {
using namespace rhythm;
void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
editor::Snapshot Base() {
    graph::Registry registry;
    editor::Snapshot snapshot;
    auto& doc = snapshot.document_;
    doc.id_ = "scene-edit-contract";
    doc.nodes_ = {registry.MakeNode(1, "geometry.cube"),   registry.MakeNode(2, "scene.instance"),
                  registry.MakeNode(3, "scene.transform"), registry.MakeNode(4, "scene.transform"),
                  registry.MakeNode(5, "scene.camera"),    registry.MakeNode(6, "scene.render"),
                  registry.MakeNode(7, "output.texture")};
    doc.nodes_[2].properties_["rotation_y"] = 120.0;
    doc.nodes_[2].properties_["scale"] = 2.0;
    doc.nodes_[3].properties_["rotation_z"] = 30.0;
    doc.nodes_[3].properties_["scale_x"] = 2.0;
    doc.nodes_[4].properties_["projection"] = 1.0;
    doc.edges_ = {{1, 1, 2, "geometry"}, {2, 2, 3, "scene"},  {3, 3, 4, "scene"},
                  {4, 4, 6, "scene"},    {5, 5, 6, "camera"}, {6, 6, 7, "source"}};
    doc.output_ = 7;
    return snapshot;
}
void Run() {
    const auto base = Base();
    const auto target = std::get<editor::SceneTarget>(editor::InspectSceneTarget(base, 3));
    auto named = base;
    for (const auto& edge : named.document_.edges_) {
        const auto name = "source-" + std::to_string(edge.from_);
        named.document_.signals_.push_back({name, edge.from_});
        named.document_.bindings_.push_back({edge.to_, edge.input_, name});
    }
    named.document_.edges_.clear();
    const auto named_target = std::get<editor::SceneTarget>(editor::InspectSceneTarget(named, 3));
    Check(named_target.parent_ == target.parent_ &&
                  named_target.render_node_ == target.render_node_ &&
                  named_target.scene_source_ == target.scene_source_ &&
                  named_target.camera_.kind_ == target.camera_.kind_,
          "named scene, image and camera routes retain the same author coordinates");
    editor::SceneEdit named_edit(named, 3);
    auto named_pose = named_target.pose_;
    named_pose.translation_.x_ = .5;
    Check(named_edit.Update(scene::ComposeEuler(named_pose)), "move through named scene route");
    const auto named_result = std::get<editor::Snapshot>(named_edit.Finish(named));
    Check(named_result.document_.bindings_ == named.document_.bindings_ &&
                  named_result.document_.edges_.empty(),
          "direct editing must preserve authored named routes");
    named.document_.signals_.clear();
    Check(std::holds_alternative<graph::Diagnostic>(editor::InspectSceneTarget(named, 3)),
          "unresolved scene routes cannot authorize an edit");
    Check(target.render_node_ == 6 && target.scene_source_ == 4 &&
                  target.camera_.kind_ == scene::ProjectionKind::kOrthographic,
          "trace render/camera scope");
    const auto expected_parent = scene::ComposeEuler({{}, {0, 0, 30}, {2, 1, 1}});
    Check(target.parent_ == expected_parent, "downstream parent transform");
    editor::SceneEdit edit(base, 3);
    Check(!edit.Update(scene::ComposeEuler(target.pose_)) && edit.Error().empty(),
          "no-op preserves noncanonical Euler authorship");
    auto pose = target.pose_;
    pose.translation_ = {.3, .4, .2};
    Check(edit.Update(scene::ComposeEuler(pose)), "matrix writes changed translation");
    Check(graph::Scalar(edit.Preview().document_.nodes_[2], "rotation_y", 0) == 120,
          "translation leaves original Euler branch untouched");
    pose.translation_.x_ = .6;
    Check(edit.Update(scene::ComposeEuler(pose)), "many motions same transaction");
    editor::History history(base);
    Check(history.Apply(std::get<editor::Snapshot>(edit.Finish(history.Current())), 0),
          "commit once");
    Check(history.Undo() && history.Current().document_.nodes_ == base.document_.nodes_ &&
                  !history.Undo(),
          "one undo restores complete author transform");
    Check(std::holds_alternative<graph::Diagnostic>(edit.Finish(history.Current())),
          "stale commit rejects");
    auto shear = scene::ComposeEuler(target.pose_);
    shear.values_[4] += .2;
    const auto previous = edit.Preview();
    Check(!edit.Update(shear) && edit.Error() == "scene_edit.non_trs" &&
                  edit.Preview() == previous &&
                  std::holds_alternative<graph::Diagnostic>(edit.Finish(base)),
          "sheared result cannot silently modify or commit prior draft");
    pose.translation_.x_ = 2000;
    Check(!edit.Update(scene::ComposeEuler(pose)) && edit.Error() == "scene_edit.range",
          "translation bounds");
    pose.translation_ = {};
    pose.scale_.x_ = 200;
    Check(!edit.Update(scene::ComposeEuler(pose)) && edit.Error() == "scene_edit.range",
          "effective scale must render identically");
    auto driven = base;
    driven.document_.nodes_[2].properties_["rotation_x"] = parameters::Expression("time");
    Check(std::get<graph::Diagnostic>(editor::InspectSceneTarget(driven, 3)).code_ ==
                  "scene_edit.driven",
          "expression cannot be overwritten");
    driven = base;
    driven.document_.nodes_.push_back(graph::Registry{}.MakeNode(8, "scalar.constant"));
    driven.document_.signals_.push_back({"music", 8});
    driven.document_.bindings_.push_back({4, "scale", "music"});
    Check(std::get<graph::Diagnostic>(editor::InspectSceneTarget(driven, 3)).code_ ==
                  "scene_edit.driven",
          "driven downstream parent rejects");
    graph::Registry registry;
    auto warped = base;
    warped.document_.nodes_.push_back(registry.MakeNode(8, "texture.affine"));
    warped.document_.edges_.back().to_ = 8;
    warped.document_.edges_.push_back({7, 8, 7, "source"});
    Check(std::get<graph::Diagnostic>(editor::InspectSceneTarget(warped, 3)).code_ ==
                  "scene_edit.image_warp",
          "image warp cannot pretend to be camera viewport");
    auto duplicate = base;
    duplicate.document_.nodes_.push_back(registry.MakeNode(8, "scene.merge"));
    duplicate.document_.edges_[3] = {4, 4, 8, "a"};
    duplicate.document_.edges_.push_back({7, 3, 8, "b"});
    duplicate.document_.edges_.push_back({8, 8, 6, "scene"});
    Check(std::get<graph::Diagnostic>(editor::InspectSceneTarget(duplicate, 3)).code_ ==
                  "scene_edit.ambiguous_route",
          "duplicated author output requires explicit instance scope");
    std::cout << "Scene editing: author/render/camera route, unchanged Euler branch, one undo, "
                 "driven/stale/shear/range protection passed\n";
}
}  // namespace
int main() {
    try {
        Run();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
