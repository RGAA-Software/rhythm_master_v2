#pragma once

#include "rhythm/render/renderer.h"

namespace rhythm::player {
enum class RenderQuality { kOriginal, kBalanced, kEconomy };
// Never upscale or change the authored canvas. The host presents this temporary
// render extent using the original aspect ratio; a quality change resets GPU
// history through Session's existing extent-change path.
render::Extent PlaybackExtent(render::Extent canvas, RenderQuality quality);
}  // namespace rhythm::player
