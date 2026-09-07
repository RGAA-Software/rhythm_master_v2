#include "rhythm/editor/component_edit.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

#include "rhythm/graph/components.h"

namespace rhythm::editor {
ComponentEdit::ComponentEdit(Snapshot project, std::string type)
    : project_id_(project.document_.id_),
      project_revision_(project.document_.revision_),
      history_(std::move(project)),
      path_{std::move(type)} {
    (void)Definition();
}
const graph::ComponentDefinition& ComponentEdit::Definition() const {
    const auto& library = history_.Current().document_.components_;
    const auto found = std::find_if(library.begin(), library.end(),
                                    [&](const auto& value) { return value.type_ == path_.back(); });
    if (found == library.end()) throw std::invalid_argument("graph.component");
    return *found;
}
Snapshot ComponentEdit::Body() const {
    const auto& definition = Definition();
    const auto& project = history_.Current();
    Snapshot result;
    result.document_.id_ = project.document_.id_;
    result.document_.revision_ = project.document_.revision_;
    result.document_.canvas_ = project.document_.canvas_;
    result.document_.nodes_ = definition.nodes_;
    result.document_.edges_ = definition.edges_;
    result.document_.output_ = definition.output_;
    result.document_.signals_ = definition.signals_;
    result.document_.bindings_ = definition.bindings_;
    result.document_.components_ = project.document_.components_;
    result.title_ = definition.title_;
    result.assets_ = project.assets_;
    if (const auto layout = project.component_positions_.find(definition.type_);
        layout != project.component_positions_.end())
        result.positions_ = layout->second;
    std::size_t index = 0;
    for (const auto& node : definition.nodes_) {
        result.positions_.try_emplace(node.id_,
                                      Position{40 + 300.0f * static_cast<float>(index % 3),
                                               40 + 250.0f * static_cast<float>(index / 3)});
        ++index;
    }
    return result;
}
bool ComponentEdit::ReplaceBody(Snapshot body) {
    const auto& current = history_.Current();
    if (body.document_.id_ != project_id_ ||
        body.document_.revision_ != current.document_.revision_)
        return false;
    auto next = current;
    const auto found =
            std::find_if(next.document_.components_.begin(), next.document_.components_.end(),
                         [&](const auto& value) { return value.type_ == path_.back(); });
    found->nodes_ = std::move(body.document_.nodes_);
    found->edges_ = std::move(body.document_.edges_);
    found->output_ = body.document_.output_;
    found->signals_ = std::move(body.document_.signals_);
    found->bindings_ = std::move(body.document_.bindings_);
    next.component_positions_[path_.back()] = std::move(body.positions_);
    return history_.Apply(std::move(next), current.document_.revision_);
}
bool ComponentEdit::ReplaceInterface(graph::ComponentDefinition definition) {
    if (definition.type_ != path_.back()) return false;
    auto next = history_.Current();
    const auto found =
            std::find_if(next.document_.components_.begin(), next.document_.components_.end(),
                         [&](const auto& value) { return value.type_ == path_.back(); });
    found->inputs_ = std::move(definition.inputs_);
    found->parameters_ = std::move(definition.parameters_);
    found->title_ = std::move(definition.title_);
    const auto revision = next.document_.revision_;
    return history_.Apply(std::move(next), revision);
}
bool ComponentEdit::Enter(graph::NodeId instance) {
    if (path_.size() >= 16) return false;
    const auto& nodes = Definition().nodes_;
    const auto found = std::find_if(nodes.begin(), nodes.end(),
                                    [&](const auto& value) { return value.id_ == instance; });
    if (found == nodes.end() || std::find(path_.begin(), path_.end(), found->type_) != path_.end())
        return false;
    const auto& library = history_.Current().document_.components_;
    if (std::none_of(library.begin(), library.end(),
                     [&](const auto& value) { return value.type_ == found->type_; }))
        return false;
    path_.push_back(found->type_);
    return true;
}
void ComponentEdit::Navigate(std::size_t index) {
    if (index < path_.size()) path_.resize(index + 1);
}
void ComponentEdit::RepairPath() {
    const auto& library = history_.Current().document_.components_;
    while (path_.size() > 1 && std::none_of(library.begin(), library.end(), [&](const auto& value) {
               return value.type_ == path_.back();
           }))
        path_.pop_back();
}
bool ComponentEdit::Undo() {
    const bool changed = history_.Undo();
    RepairPath();
    return changed;
}
bool ComponentEdit::Redo() {
    const bool changed = history_.Redo();
    RepairPath();
    return changed;
}
graph::NodeId ComponentEdit::ReserveNodeId() {
    auto& next = next_ids_[path_.back()];
    for (const auto& node : Definition().nodes_) next = std::max(next, node.id_);
    if (next == std::numeric_limits<graph::NodeId>::max())
        throw std::overflow_error("graph.id_exhausted");
    return ++next;
}
EditResult ComponentEdit::Finish(const Snapshot& current, const graph::Registry& registry) const {
    if (current.document_.id_ != project_id_ || current.document_.revision_ != project_revision_)
        return graph::Diagnostic{"component.conflict"};
    auto next = current;
    next.document_.components_ = history_.Current().document_.components_;
    next.component_positions_ = history_.Current().component_positions_;
    const auto prune_hidden_parameters = [&](graph::Node& node) {
        const auto previous = std::find_if(
                current.document_.components_.begin(), current.document_.components_.end(),
                [&](const auto& value) { return value.type_ == node.type_; });
        const auto replacement =
                std::find_if(next.document_.components_.begin(), next.document_.components_.end(),
                             [&](const auto& value) { return value.type_ == node.type_; });
        if (previous == current.document_.components_.end() ||
            replacement == next.document_.components_.end())
            return;
        std::erase_if(node.properties_, [&](const auto& item) {
            return std::any_of(
                           previous->parameters_.begin(), previous->parameters_.end(),
                           [&](const auto& parameter) { return parameter.key_ == item.first; }) &&
                   std::none_of(
                           replacement->parameters_.begin(), replacement->parameters_.end(),
                           [&](const auto& parameter) { return parameter.key_ == item.first; });
        });
    };
    for (auto& node : next.document_.nodes_) prune_hidden_parameters(node);
    for (auto& definition : next.document_.components_)
        for (auto& node : definition.nodes_) prune_hidden_parameters(node);
    // Validate expanded structure, including unused instances. Incomplete root
    // authoring is allowed, but invalid types/interfaces and recursion are not.
    auto expansion = graph::ExpandComponents(next.document_, registry);
    if (std::holds_alternative<std::vector<graph::Diagnostic>>(expansion))
        return std::get<std::vector<graph::Diagnostic>>(expansion).front();
    const auto compiled = graph::Compile(next.document_, registry);
    if (std::holds_alternative<std::vector<graph::Diagnostic>>(compiled)) {
        for (const auto& diagnostic : std::get<std::vector<graph::Diagnostic>>(compiled))
            if (diagnostic.code_ != "graph.required_input" &&
                diagnostic.code_ != "graph.missing_output" &&
                diagnostic.code_ != "graph.output_type")
                return diagnostic;
    }
    return next;
}
}  // namespace rhythm::editor
