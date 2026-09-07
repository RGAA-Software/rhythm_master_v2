#pragma once

#include "backend.h"

namespace rhythm::render::detail {
// Only the platform adapter calls this private boundary. The shared owner keeps
// SDL and its window alive until bgfx shutdown, including outstanding textures.
std::shared_ptr<Backend> CreateBgfxBackend(std::uintptr_t native_window, Extent size,
                                           std::shared_ptr<void> surface_owner,
                                           std::uintptr_t external_context = 0);
}  // namespace rhythm::render::detail
