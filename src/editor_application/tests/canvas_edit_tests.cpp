#include <cmath>
#include <iostream>
#include <stdexcept>

#include "rhythm/editor/canvas_edit.h"

namespace {
using namespace rhythm;
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
editor::Snapshot Base() {
    graph::Registry registry;
    editor::Snapshot result;
    auto& document = result.document_;
    document.id_ = "canvas-draft";
    document.canvas_ = {200, 100};
    document.nodes_ = {
            registry.MakeNode(1, "texture.gradient"), registry.MakeNode(2, "texture.affine"),
            registry.MakeNode(3, "texture.affine"), registry.MakeNode(4, "output.texture")};
    document.nodes_[2].properties_["scale_x"] = -2.0;
    document.nodes_[2].properties_["scale_y"] = 0.5;
    document.nodes_[2].properties_["rotation"] = 30.0;
    document.edges_ = {{1, 1, 2, "source"}, {2, 2, 3, "source"}, {3, 3, 4, "source"}};
    document.output_ = 4;
    return result;
}
void Run() {
    const auto base = Base();
    const auto target = std::get<editor::CanvasTarget>(editor::InspectCanvasTarget(base, 2));
    const auto screen = [&](geometry2d::Point point) {
        return geometry2d::Transform(target.parent_, point);
    };
    editor::CanvasEdit move(base, 2, editor::CanvasGesture::kTranslate, screen({100, 50}));
    Require(move.Update(screen({117, 71}), {10, 0, 0}), "move under mirrored nonuniform parent");
    auto edited = std::get<editor::Snapshot>(move.Finish(base));
    Require(std::abs(graph::Scalar(edited.document_.nodes_[1], "translate_x", 0) - .1) < 1e-8 &&
                    std::abs(graph::Scalar(edited.document_.nodes_[1], "translate_y", 0) - .2) <
                            1e-8,
            "pixel snap in author coordinates");
    Require(move.Update(screen({137, 91}), {10, 0, 0}), "second motion updates same draft");
    editor::History history(base);
    Require(history.Apply(std::get<editor::Snapshot>(move.Finish(history.Current())), 0),
            "commit one gesture");
    Require(history.Undo() && history.Current().document_.nodes_ == base.document_.nodes_ &&
                    !history.Undo(),
            "many mouse motions produce exactly one history transaction");
    Require(std::holds_alternative<graph::Diagnostic>(move.Finish(history.Current())),
            "stale revision cannot overwrite undo");
    editor::CanvasEdit rotate(base, 2, editor::CanvasGesture::kRotate, screen({150, 50}));
    rotate.Update(screen({100, 100}), {0, 15, 0});
    Require(std::abs(graph::Scalar(rotate.Preview().document_.nodes_[1], "rotation", 0) - 90) <
                    1e-8,
            "rotation uses inverse parent coordinates");
    editor::CanvasEdit scale(base, 2, editor::CanvasGesture::kScale, screen({200, 100}));
    scale.Update(screen({0, 150}));
    Require(std::abs(graph::Scalar(scale.Preview().document_.nodes_[1], "scale_x", 0) + 1) < 1e-8 &&
                    std::abs(graph::Scalar(scale.Preview().document_.nodes_[1], "scale_y", 0) - 2) <
                            1e-8,
            "scale crosses axis and keeps independent axes");
    auto pivot_base = base;
    pivot_base.document_.nodes_[1].properties_["scale"] = 2.0;
    const auto original =
            std::get<editor::CanvasTarget>(editor::InspectCanvasTarget(pivot_base, 2));
    editor::CanvasEdit pivot(pivot_base, 2, editor::CanvasGesture::kPivot, screen({100, 50}));
    pivot.Update(screen({140, 70}));
    const auto changed =
            std::get<editor::CanvasTarget>(editor::InspectCanvasTarget(pivot.Preview(), 2));
    const auto before = geometry2d::Compose(original.pose_, original.canvas_);
    const auto after = geometry2d::Compose(changed.pose_, changed.canvas_);
    for (std::size_t index = 0; index < before.values_.size(); ++index)
        Require(std::abs(before.values_[index] - after.values_[index]) < 1e-8,
                "pivot adjustment preserves output mapping");
    auto driven = base;
    driven.document_.bindings_.push_back({2, "rotation", "music"});
    Require(std::get<graph::Diagnostic>(editor::InspectCanvasTarget(driven, 2)).code_ ==
                    "canvas.driven_transform",
            "binding cannot be silently overwritten");
    auto singular = base;
    singular.document_.nodes_[2].properties_["scale_x"] = 0.0;
    Require(std::holds_alternative<graph::Diagnostic>(editor::InspectCanvasTarget(singular, 2)),
            "singular parent rejects manipulation");
    auto hidden = base;
    hidden.document_.nodes_[1].properties_["opacity"] = 0.0;
    Require(std::get<graph::Diagnostic>(editor::InspectCanvasTarget(hidden, 2)).code_ ==
                    "canvas.invisible",
            "hidden target cannot be picked");
    auto ambiguous = base;
    ambiguous.document_.edges_.push_back({4, 2, 4, "source"});
    Require(std::get<graph::Diagnostic>(editor::InspectCanvasTarget(ambiguous, 2)).code_ ==
                    "canvas.ambiguous_route",
            "ambiguous output route rejects");
    Require(base.document_.revision_ == 0 &&
                    graph::Scalar(base.document_.nodes_[1], "translate_x", 0) == 0,
            "discarding value drafts leaves base untouched");
}
}  // namespace
int main() {
    try {
        Run();
        std::cout << "Canvas edits: parent coordinates, four gestures, snap, one undo, stale and "
                     "driven protection passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
