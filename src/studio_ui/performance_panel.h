#pragma once

#include <map>
#include <string>

#include "rhythm/runtime/runtime.h"

namespace rhythm::studio {
// Returns whether per-node instrumentation is requested for the next frame.
bool DrawPerformancePanel(const runtime::FrameResult& frame, const render::FrameStats& stats,
                          graph::NodeId selected, bool& reuse_textures,
                          const std::map<std::string, std::string>& text);
}  // namespace rhythm::studio
