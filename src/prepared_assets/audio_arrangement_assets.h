#pragma once

#include "rhythm/prepared_assets/prepare.h"

namespace rhythm::prepared_assets::detail {
media::SoundtrackSource PrepareArrangement(const media::Soundtrack& binding,
                                           std::span<const project::PackagedAsset> assets,
                                           std::stop_token stop);
}  // namespace rhythm::prepared_assets::detail
