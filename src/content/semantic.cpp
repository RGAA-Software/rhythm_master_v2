#include "rhythm/content/semantic.h"

#include <algorithm>
#include <set>
#include <stdexcept>

namespace rhythm::content {
std::vector<Semantic> LoadSemantics(const std::filesystem::path& root,
                                    const graph::Registry& registry) {
    const auto entries = project::ScanTemplates(root);
    if (entries.size() > 128) throw std::length_error("content.semantic_limit");
    std::vector<Semantic> result;
    std::set<std::string> types;
    std::size_t total_nodes = 0;
    for (const auto& entry : entries) {
        auto loaded = project::LoadRevision(entry.directory_);
        const auto& document = loaded.snapshot_.document_;
        if (!loaded.warnings_.empty() || !loaded.snapshot_.assets_.empty() ||
            document.nodes_.size() != 2 || document.edges_.size() != 1 ||
            !document.signals_.empty() || !document.bindings_.empty() ||
            !std::holds_alternative<graph::ExecutionPlan>(graph::Compile(document, registry)))
            throw std::invalid_argument("content.semantic_graph");
        const auto& edge = document.edges_.front();
        const auto found = std::find_if(document.nodes_.begin(), document.nodes_.end(),
                                        [&](const auto& node) { return node.id_ == edge.from_; });
        if (found == document.nodes_.end() || edge.to_ != document.output_ ||
            edge.input_ != "source" || !found->type_.starts_with("component.official.") ||
            !types.insert(found->type_).second)
            throw std::invalid_argument("content.semantic_root");
        const auto descriptor = registry.Find(found->type_, document.components_);
        if (!descriptor || descriptor->operation_ != graph::Operation::kComponent ||
            descriptor->properties_.empty())
            throw std::invalid_argument("content.semantic_interface");
        for (const auto& definition : document.components_) total_nodes += definition.nodes_.size();
        if (total_nodes > 10000) throw std::length_error("content.semantic_limit");
        const auto node = *found;
        auto presets =
                LoadPresets(entry.directory_ / "presets.json", registry, document.components_);
        if (presets.empty() || presets.size() > 32 ||
            std::none_of(presets.begin(), presets.end(),
                         [](const auto& preset) {
                             return preset.id_.ends_with(".default") && preset.reset_;
                         }) ||
            std::any_of(presets.begin(), presets.end(),
                        [&](const auto& preset) { return preset.operator_type_ != node.type_; }))
            throw std::invalid_argument("content.semantic_presets");
        result.push_back({entry, std::move(loaded.snapshot_), node, std::move(presets)});
    }
    return result;
}
editor::EditResult AddSemantic(const editor::Snapshot& snapshot, const Semantic& semantic,
                               const graph::Registry& registry, editor::Position position,
                               graph::NodeId id) {
    auto next = snapshot;
    for (const auto& definition : semantic.content_.document_.components_) {
        const auto found =
                std::find_if(next.document_.components_.begin(), next.document_.components_.end(),
                             [&](const auto& value) { return value.type_ == definition.type_; });
        if (found != next.document_.components_.end()) {
            if (*found != definition) return graph::Diagnostic{"content.semantic_conflict"};
            continue;
        }
        next.document_.components_.push_back(definition);
        if (const auto layout = semantic.content_.component_positions_.find(definition.type_);
            layout != semantic.content_.component_positions_.end())
            next.component_positions_[definition.type_] = layout->second;
    }
    std::size_t nodes = next.document_.nodes_.size() + 1;
    std::size_t edges = next.document_.edges_.size() + next.document_.bindings_.size();
    for (const auto& definition : next.document_.components_) {
        nodes += definition.nodes_.size();
        edges += definition.edges_.size() + definition.bindings_.size();
    }
    if (next.document_.components_.size() > 256 || nodes > 10000 || edges > 40000)
        return graph::Diagnostic{"graph.limit"};
    auto result = editor::AddNode(next, registry, semantic.root_.type_, position, id);
    if (std::holds_alternative<editor::Snapshot>(result)) {
        auto& inserted = std::get<editor::Snapshot>(result).document_.nodes_.back();
        for (const auto& [key, value] : semantic.root_.properties_)
            inserted.properties_[key] = value;
        if (const auto errors = registry.ValidateNode(inserted, next.document_.components_);
            !errors.empty())
            return errors.front();
    }
    return result;
}
}  // namespace rhythm::content
