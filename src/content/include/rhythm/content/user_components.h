#pragma once

#include "rhythm/content/semantic.h"

namespace rhythm::content {
// Capture one component instance, its complete nested definition/layout closure
// and referenced immutable assets. Public parameters keep this instance's values.
// Library definitions receive content-derived names, avoiding unrelated projects'
// local component.user.N collisions. No unrelated graph nodes/signals are stored.
editor::Snapshot CaptureComponent(const editor::Snapshot& snapshot, graph::NodeId instance,
                                  const graph::Registry& registry);
editor::EditResult InsertComponent(const editor::Snapshot& snapshot,
                                   const editor::Snapshot& component,
                                   const graph::Registry& registry, editor::Position position,
                                   graph::NodeId id);
// Blocking operations for the library worker. Reuse the project transaction,
// Protobuf/layout codecs and immutable asset store; no second persistence format.
std::filesystem::path SaveComponent(const std::filesystem::path& library,
                                    const editor::Snapshot& component,
                                    const std::filesystem::path& source_assets);
editor::Snapshot LoadComponent(const std::filesystem::path& directory,
                               const std::filesystem::path& destination_assets);
}  // namespace rhythm::content
