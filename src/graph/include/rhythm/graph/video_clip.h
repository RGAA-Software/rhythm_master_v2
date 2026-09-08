#pragma once

#include "rhythm/graph/registry.h"
#include "rhythm/parameters/clip_interval.h"

namespace rhythm::graph {
// Validates the relationship between placement, trim and playback properties.
// No decoder, platform or clock state belongs to this graph contract.
parameters::ClipInterval DescribeVideoClip(const Node& node);
}  // namespace rhythm::graph
