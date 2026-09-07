#pragma once

#include <functional>

#include "rhythm/export/settings.h"
#include "rhythm/project/package.h"
#include "rhythm/render/renderer.h"

namespace rhythm::exporting {
// Dedicated offline worker with its own host/device, never the live Studio
// presentation thread. Reuses the shared graph runtime and media adapters.
// Three GPU tickets, two queued encoding frames and one active encoding frame;
// at most one frame's PCM per image, with no whole-track decode.
// The caller owns staging cleanup and publication after this function succeeds.
void RenderExport(const project::RuntimePackage& package, const ExportSettings& settings,
                  const std::filesystem::path& staging, render::Renderer& renderer,
                  const std::function<void(ExportProgress)>& progress = {},
                  std::stop_token stop = {});
}  // namespace rhythm::exporting
