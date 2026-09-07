#pragma once

#include "rhythm/render/renderer.h"

namespace rhythm::render {
// Fits without cropping or stretching, centered within the destination bounds.
ClipRect AspectFit(Extent source, ClipRect bounds);
}  // namespace rhythm::render
