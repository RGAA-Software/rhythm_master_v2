#include "rhythm/editor/soundtrack_command.h"

#include <set>
#include <stdexcept>

namespace rhythm::editor {
Snapshot WithSoundtrack(Snapshot snapshot, std::optional<media::Soundtrack> soundtrack) {
    if (soundtrack && !media::ValidSoundtrack(*soundtrack, snapshot.assets_))
        throw std::invalid_argument("project.soundtrack_invalid");
    std::set<std::string> retired;
    if (snapshot.soundtrack_) {
        retired.insert(snapshot.soundtrack_->asset_.sha256_);
        for (const auto& clip : snapshot.soundtrack_->clips_) retired.insert(clip.asset_.sha256_);
    }
    if (soundtrack) {
        retired.erase(soundtrack->asset_.sha256_);
        for (const auto& clip : soundtrack->clips_) retired.erase(clip.asset_.sha256_);
    }
    const auto retain = [&](std::span<const graph::Node> nodes) {
        for (const auto& node : nodes)
            for (const auto& [key, value] : node.properties_)
                if (std::holds_alternative<assets::AssetId>(value))
                    retired.erase(std::get<assets::AssetId>(value).sha256_);
    };
    retain(snapshot.document_.nodes_);
    for (const auto& component : snapshot.document_.components_) retain(component.nodes_);
    std::erase_if(snapshot.assets_,
                  [&](const auto& record) { return retired.contains(record.id_.sha256_); });
    snapshot.soundtrack_ = std::move(soundtrack);
    return snapshot;
}
}  // namespace rhythm::editor
