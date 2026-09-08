#pragma once

#include "rhythm/editor/history.h"

namespace rhythm::editor {
// Replaces the binding and removes only retired music records no longer used by
// any graph/component or the new mix. Immutable blobs remain available to undo.
Snapshot WithSoundtrack(Snapshot snapshot, std::optional<media::Soundtrack> soundtrack);
}  // namespace rhythm::editor
