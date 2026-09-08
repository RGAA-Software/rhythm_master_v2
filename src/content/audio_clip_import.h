#pragma once

#include "rhythm/assets/store.h"
#include "rhythm/editor/history.h"

namespace rhythm::content::detail {
// Worker-only duration probing and value construction. Caller owns asset import,
// publication budgets and the revision-checked editor transaction.
media::Soundtrack AppendMusic(const editor::Snapshot& snapshot, const assets::AssetRecord& record,
                              std::string title, const assets::Store& store, float gain, bool loop,
                              std::stop_token stop);
}  // namespace rhythm::content::detail
