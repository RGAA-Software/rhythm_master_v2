#pragma once

#include <map>

#include "rhythm/render/renderer.h"

namespace rhythm::platform {
render::DrawList TranslateUi(const std::map<std::uint64_t, render::TextureHandle>& textures);
}
