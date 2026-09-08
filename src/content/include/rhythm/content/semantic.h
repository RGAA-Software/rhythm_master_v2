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
// Official entries use a bounded executable harness: one embedded component
// wired directly to texture output, with optional primitive demonstration inputs.
// Demonstration nodes remain in the preview; insertion copies the component only.
// Cold-start loading validates all entries; insertion is an in-memory edit.
std::vector<Semantic> LoadSemantics(const std::filesystem::path& root,
                                    const graph::Registry& registry);
// Embed immutable-by-default definitions and layout as one undoable edit. An
// existing locally edited definition is never silently overwritten by a catalog.
// Asset-bearing entries must first use LoadOfficialComponent on a worker, then
// InsertComponent to merge prepared records atomically with the graph edit.
editor::EditResult AddSemantic(const editor::Snapshot& snapshot, const Semantic& semantic,
                               const graph::Registry& registry, editor::Position position,
                               graph::NodeId id);
}  // namespace rhythm::content
