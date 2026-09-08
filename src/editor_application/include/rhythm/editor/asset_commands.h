#pragma once

#include "rhythm/editor/commands.h"

namespace rhythm::editor {
enum class AssetUseKind { kNode, kSoundtrack, kAudioClip };
struct AssetUse {
    AssetUseKind kind_ = AssetUseKind::kNode;
    std::string component_{};
    graph::NodeId node_ = 0;
    std::string property_{};
    std::uint64_t clip_ = 0;
};
// Pure authored-reference inspection, including inactive component definitions.
// No filesystem paths or prepared resources participate in these commands.
std::vector<AssetUse> AssetUses(const Snapshot& snapshot, const assets::AssetId& id);
EditResult ReplaceAsset(const Snapshot& snapshot, const assets::AssetId& previous,
                        const assets::AssetRecord& replacement);
EditResult RemoveUnusedAsset(const Snapshot& snapshot, const assets::AssetId& id);
}  // namespace rhythm::editor
