#include "rhythm/content/user_components.h"

#include <algorithm>
#include <set>
#include <stdexcept>

#include "rhythm/assets/store.h"
#include "rhythm/graph/components.h"
#include "rhythm/project/package.h"

namespace rhythm::content {
namespace {
void Validate(const editor::Snapshot& component, const graph::Registry& registry) {
    const auto& document = component.document_;
    if (document.id_ != "component.library" || document.nodes_.size() != 1 ||
        document.output_ != document.nodes_.front().id_ || !document.edges_.empty() ||
        !document.signals_.empty() || !document.bindings_.empty())
        throw std::invalid_argument("component.library_invalid");
    const auto descriptor = registry.Find(document.nodes_.front().type_, document.components_);
    if (!descriptor || descriptor->operation_ != graph::Operation::kComponent ||
        !registry.ValidateNode(document.nodes_.front(), document.components_).empty() ||
        !std::holds_alternative<graph::Document>(graph::ExpandComponents(document, registry)))
        throw std::invalid_argument("component.library_invalid");
}
void CopyAssets(const editor::Snapshot& component, const std::filesystem::path& from,
                const std::filesystem::path& to) {
    if (component.assets_.empty()) return;
    if (component.assets_.size() > project::kMaximumPackageAssets)
        throw std::length_error("package.asset_count");
    assets::Store source(from), destination(to);
    std::uint64_t remaining = project::kMaximumPackageAssetBytes;
    for (const auto& asset : component.assets_) {
        destination.CopyFrom(source, asset, remaining);
        remaining -= asset.bytes_;
    }
}
}  // namespace
editor::Snapshot CaptureComponent(const editor::Snapshot& snapshot, graph::NodeId instance,
                                  const graph::Registry& registry) {
    const auto& document = snapshot.document_;
    const auto root = std::find_if(document.nodes_.begin(), document.nodes_.end(),
                                   [&](const auto& node) { return node.id_ == instance; });
    if (root == document.nodes_.end()) throw std::invalid_argument("graph.missing_node");
    const auto descriptor = registry.Find(root->type_, document.components_);
    if (!descriptor || descriptor->operation_ != graph::Operation::kComponent)
        throw std::invalid_argument("component.library_invalid");
    editor::Snapshot result;
    result.document_.id_ = "component.library";
    result.document_.canvas_ = document.canvas_;
    result.document_.output_ = 1;
    result.document_.nodes_ = {*root};
    result.document_.nodes_.front().id_ = 1;
    result.positions_[1] = {40, 40};
    // Focused reuse of the existing DetachComponent closure walk: each nested
    // definition is visited once, retaining internal IDs and editor layout.
    std::vector<std::string> pending{root->type_};
    std::set<std::string> visited{root->type_};
    for (std::size_t index = 0; index < pending.size(); ++index) {
        if (pending.size() > 256) throw std::length_error("graph.limit");
        const auto found =
                std::find_if(document.components_.begin(), document.components_.end(),
                             [&](const auto& value) { return value.type_ == pending[index]; });
        if (found == document.components_.end())
            throw std::invalid_argument("component.library_invalid");
        result.document_.components_.push_back(*found);
        if (index == 0) result.title_ = found->title_;
        if (const auto layout = snapshot.component_positions_.find(found->type_);
            layout != snapshot.component_positions_.end())
            result.component_positions_[found->type_] = layout->second;
        for (const auto& child : found->nodes_)
            if (child.type_.starts_with("component.") && visited.insert(child.type_).second)
                pending.push_back(child.type_);
    }
    std::set<std::string> asset_ids;
    const auto collect = [&](const graph::Node& node) {
        for (const auto& [key, value] : node.properties_)
            if (const auto asset = std::get_if<assets::AssetId>(&value);
                asset && !asset->sha256_.empty())
                asset_ids.insert(asset->sha256_);
    };
    collect(result.document_.nodes_.front());
    for (const auto& definition : result.document_.components_)
        for (const auto& node : definition.nodes_) collect(node);
    for (const auto& id : asset_ids) {
        const auto asset = std::find_if(snapshot.assets_.begin(), snapshot.assets_.end(),
                                        [&](const auto& value) { return value.id_.sha256_ == id; });
        if (asset == snapshot.assets_.end()) throw std::invalid_argument("project.asset_reference");
        result.assets_.push_back(*asset);
    }
    Validate(result, registry);
    auto canonical = result.document_;
    std::map<std::string, std::string> canonical_names;
    for (std::size_t index = 0; index < pending.size(); ++index)
        canonical_names[pending[index]] = "component.user.capture." + std::to_string(index);
    canonical.nodes_.front().type_ = canonical_names.at(root->type_);
    for (auto& definition : canonical.components_) {
        definition.type_ = canonical_names.at(definition.type_);
        for (auto& node : definition.nodes_)
            if (canonical_names.contains(node.type_)) node.type_ = canonical_names.at(node.type_);
    }
    const auto prefix = "component.user.library." +
                        project::Digest(project::EncodeGraph(canonical)).substr(0, 24);
    std::map<std::string, std::string> names;
    for (std::size_t index = 0; index < pending.size(); ++index)
        names[pending[index]] = prefix + "." + std::to_string(index);
    result.document_.nodes_.front().type_ = names.at(root->type_);
    for (auto& definition : result.document_.components_) {
        definition.type_ = names.at(definition.type_);
        for (auto& node : definition.nodes_)
            if (names.contains(node.type_)) node.type_ = names.at(node.type_);
    }
    auto positions = std::move(result.component_positions_);
    result.component_positions_.clear();
    for (auto& [name, layout] : positions)
        result.component_positions_[names.at(name)] = std::move(layout);
    // Normalize codec-owned extension records now, so an in-memory capture and
    // its persisted/reloaded definition compare identically without discarding
    // forward-compatible unknown fields.
    result.document_ = project::DecodeGraph(project::EncodeGraph(result.document_));
    return result;
}
editor::EditResult InsertComponent(const editor::Snapshot& snapshot,
                                   const editor::Snapshot& component,
                                   const graph::Registry& registry, editor::Position position,
                                   graph::NodeId id) {
    try {
        Validate(component, registry);
        Semantic semantic;
        semantic.content_ = component;
        semantic.root_ = component.document_.nodes_.front();
        auto result = AddSemantic(snapshot, semantic, registry, position, id);
        if (!std::holds_alternative<editor::Snapshot>(result)) return result;
        auto& next = std::get<editor::Snapshot>(result);
        for (const auto& asset : component.assets_) {
            const auto found =
                    std::find_if(next.assets_.begin(), next.assets_.end(),
                                 [&](const auto& value) { return value.id_ == asset.id_; });
            if (found == next.assets_.end())
                next.assets_.push_back(asset);
            else if (found->bytes_ != asset.bytes_ || found->media_type_ != asset.media_type_)
                return graph::Diagnostic{"project.asset_reference"};
        }
        return result;
    } catch (const std::exception&) {
        return graph::Diagnostic{"component.library_invalid"};
    }
}
std::filesystem::path SaveComponent(const std::filesystem::path& library,
                                    const editor::Snapshot& component,
                                    const std::filesystem::path& source_assets) {
    Validate(component, graph::Registry{});
    const auto digest = project::Digest(project::EncodeGraph(component.document_));
    const auto directory = library / (digest.substr(0, 24) + ".rhythmcomponent");
    CopyAssets(component, source_assets, directory / "assets");
    project::Save(directory, component);
    return directory;
}
editor::Snapshot LoadComponent(const std::filesystem::path& directory,
                               const std::filesystem::path& destination_assets) {
    auto loaded = project::Load(directory);
    if (!loaded.warnings_.empty()) throw std::invalid_argument("component.library_invalid");
    Validate(loaded.snapshot_, graph::Registry{});
    CopyAssets(loaded.snapshot_, directory / "assets", destination_assets);
    return std::move(loaded.snapshot_);
}
}  // namespace rhythm::content
