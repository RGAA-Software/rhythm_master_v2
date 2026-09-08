#include "rhythm/editor/asset_commands.h"

#include <algorithm>

namespace rhythm::editor {
namespace {
bool Compatible(std::string_view first, std::string_view second) {
    if (first == second) return true;
    for (const auto family : {"image/", "video/", "audio/"})
        if (first.starts_with(family) && second.starts_with(family)) return true;
    return false;
}
}  // namespace
std::vector<AssetUse> AssetUses(const Snapshot& snapshot, const assets::AssetId& id) {
    std::vector<AssetUse> result;
    const auto inspect = [&](std::span<const graph::Node> nodes, const std::string& component) {
        for (const auto& node : nodes)
            for (const auto& [key, value] : node.properties_)
                if (std::holds_alternative<assets::AssetId>(value) &&
                    std::get<assets::AssetId>(value) == id)
                    result.push_back({AssetUseKind::kNode, component, node.id_, key});
    };
    inspect(snapshot.document_.nodes_, {});
    for (const auto& component : snapshot.document_.components_)
        inspect(component.nodes_, component.type_);
    if (snapshot.soundtrack_) {
        if (snapshot.soundtrack_->asset_ == id) result.push_back({AssetUseKind::kSoundtrack});
        for (const auto& clip : snapshot.soundtrack_->clips_)
            if (clip.asset_ == id)
                result.push_back({AssetUseKind::kAudioClip, {}, 0, {}, clip.id_});
    }
    return result;
}
EditResult ReplaceAsset(const Snapshot& snapshot, const assets::AssetId& previous,
                        const assets::AssetRecord& replacement) {
    const auto found = std::find_if(snapshot.assets_.begin(), snapshot.assets_.end(),
                                    [&](const auto& record) { return record.id_ == previous; });
    if (found == snapshot.assets_.end()) return graph::Diagnostic{"asset.not_listed"};
    if (!assets::ValidId(replacement.id_) || !assets::ValidMediaType(replacement.media_type_) ||
        !replacement.bytes_ || !Compatible(found->media_type_, replacement.media_type_))
        return graph::Diagnostic{"asset.incompatible"};
    for (const auto& record : snapshot.assets_)
        if (record.id_ == replacement.id_ && record != replacement)
            return graph::Diagnostic{"asset.metadata_conflict"};
    if (previous == replacement.id_) return snapshot;
    auto next = snapshot;
    const auto rewrite = [&](std::span<graph::Node> nodes) {
        for (auto& node : nodes)
            for (auto& [key, value] : node.properties_)
                if (std::holds_alternative<assets::AssetId>(value) &&
                    std::get<assets::AssetId>(value) == previous)
                    value = replacement.id_;
    };
    rewrite(next.document_.nodes_);
    for (auto& component : next.document_.components_) rewrite(component.nodes_);
    if (next.soundtrack_) {
        if (next.soundtrack_->asset_ == previous) next.soundtrack_->asset_ = replacement.id_;
        for (auto& clip : next.soundtrack_->clips_)
            if (clip.asset_ == previous) clip.asset_ = replacement.id_;
    }
    std::erase_if(next.assets_, [&](const auto& record) { return record.id_ == previous; });
    if (std::none_of(next.assets_.begin(), next.assets_.end(),
                     [&](const auto& record) { return record.id_ == replacement.id_; }))
        next.assets_.push_back(replacement);
    return next;
}
EditResult RemoveUnusedAsset(const Snapshot& snapshot, const assets::AssetId& id) {
    if (!AssetUses(snapshot, id).empty()) return graph::Diagnostic{"asset.in_use"};
    auto next = snapshot;
    std::erase_if(next.assets_, [&](const auto& record) { return record.id_ == id; });
    return next;
}
}  // namespace rhythm::editor
