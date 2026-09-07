#include <iostream>
#include <stdexcept>

#include "rhythm/editor/component_edit.h"

namespace {
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void Run() {
    using namespace rhythm;
    graph::Registry registry;
    graph::ComponentDefinition inner;
    inner.type_ = "component.test.inner";
    inner.nodes_ = {registry.MakeNode(1, "texture.shape")};
    inner.output_ = 1;
    inner.parameters_ = {{"shape_width", 1, "shape_width", "Palette"}};
    graph::ComponentDefinition outer;
    outer.type_ = "component.test.outer";
    outer.nodes_ = {{1, inner.type_}};
    outer.output_ = 1;
    editor::Snapshot source;
    source.document_.id_ = "edit-component";
    source.document_.components_ = {inner, outer};
    source.document_.nodes_ = {{10, outer.type_}, registry.MakeNode(20, "output.texture")};
    source.document_.output_ = 20;
    source.document_.edges_ = {{1, 10, 20, "source"}};
    source.document_.nodes_.push_back({30, outer.type_});
    const auto detached = std::get<editor::Snapshot>(
            editor::DetachComponent(source, registry, 10, "component.user.copy"));
    Require(detached.document_.nodes_[0].type_ == "component.user.copy" &&
                    detached.document_.nodes_.back().type_ == outer.type_ &&
                    detached.document_.components_.size() == 4 &&
                    detached.document_.components_[2].nodes_[0].type_ ==
                            detached.document_.components_[3].type_ &&
                    detached.document_.components_[3].type_ != inner.type_,
            "detach isolates full nested closure for only one instance");
    Require(std::holds_alternative<graph::ExecutionPlan>(
                    graph::Compile(detached.document_, registry)) &&
                    std::holds_alternative<graph::Diagnostic>(
                            editor::DetachComponent(detached, registry, 30, "component.user.copy")),
            "detached closure compiles and name collision rejects");
    editor::ComponentEdit edit(source, outer.type_);
    Require(edit.Enter(1) && edit.Path().size() == 2, "nested navigation");
    auto body = edit.Body();
    body.document_.nodes_[0].properties_["shape_width"] = 0.6;
    body.positions_[1] = {175, 90};
    Require(edit.ReplaceBody(body), "edit nested body");
    Require(!edit.ReplaceBody(body), "stale body edit rejected");
    const auto result = edit.Finish(source, registry);
    if (std::holds_alternative<graph::Diagnostic>(result))
        throw std::runtime_error(std::get<graph::Diagnostic>(result).code_);
    auto applied = std::get<editor::Snapshot>(result);
    Require(applied.component_positions_.at(inner.type_).at(1) == editor::Position{175, 90},
            "nested layout belongs to the same project transaction");
    Require(graph::Scalar(applied.document_.components_[0].nodes_[0], "shape_width", 0) == 0.6 &&
                    graph::Scalar(source.document_.components_[0].nodes_[0], "shape_width", 0) !=
                            0.6,
            "nested edit transaction isolates source");
    Require(edit.Undo() && edit.Redo(), "shared draft history");
    auto conflicted = source;
    ++conflicted.document_.revision_;
    Require(std::get<graph::Diagnostic>(edit.Finish(conflicted, registry)).code_ ==
                    "component.conflict",
            "external revision conflict");
    const auto first = edit.ReserveNodeId();
    Require(edit.Undo() && edit.ReserveNodeId() > first, "undo never reuses local ID");
    edit.Navigate(0);
    Require(edit.Path().size() == 1, "breadcrumb back");
    body = edit.Body();
    body.document_.nodes_[0].type_ = outer.type_;
    Require(edit.ReplaceBody(body) &&
                    std::holds_alternative<graph::Diagnostic>(edit.Finish(source, registry)),
            "recursive draft cannot apply");
    Require(edit.Undo(), "invalid draft remains undoable");
    editor::ComponentEdit public_edit(source, inner.type_);
    auto definition = public_edit.Definition();
    definition.parameters_[0].maximum_ = 0.1;
    Require(public_edit.ReplaceInterface(definition) &&
                    std::holds_alternative<graph::Diagnostic>(public_edit.Finish(source, registry)),
            "invalid narrowed public default rejects");
    Require(public_edit.Undo(), "public interface undo");
    definition = public_edit.Definition();
    definition.parameters_.clear();
    Require(public_edit.ReplaceInterface(definition) &&
                    std::holds_alternative<editor::Snapshot>(public_edit.Finish(source, registry)),
            "hide public control");
    std::cout << "component editing: nested drafts, conflicts, interfaces, undo and local IDs "
                 "passed\n";
}
}  // namespace
int main() {
    try {
        Run();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
