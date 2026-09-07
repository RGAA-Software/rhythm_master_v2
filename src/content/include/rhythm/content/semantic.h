#pragma once

#include "rhythm/content/presets.h"
#include "rhythm/editor/commands.h"
#include "rhythm/project/store.h"

namespace rhythm::content {
struct Semantic {
    project::ContentEntry metadata_{};
    editor::Snapshot content_{};
    graph::Node root_{};
    std::vector<Preset> presets_{};
};
// Official semantic entries use the existing project codec and a two-node
// executable harness: one embedded component wired to one texture output.
// Cold-start loading validates all entries; insertion is an in-memory edit.
std::vector<Semantic> LoadSemantics(const std::filesystem::path& root,
                                    const graph::Registry& registry);
// Embed immutable-by-default definitions and layout as one undoable edit. An
// existing locally edited definition is never silently overwritten by a catalog.
editor::EditResult AddSemantic(const editor::Snapshot& snapshot, const Semantic& semantic,
                               const graph::Registry& registry, editor::Position position,
                               graph::NodeId id);
}  // namespace rhythm::content
