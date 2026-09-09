#include "rhythm/image_shader/program.h"

namespace rhythm::image_shader {
void ValidateArtifact(std::span<const std::uint8_t> bytes, Target target) {
    shader_artifact::Validate(bytes, target, shader_artifact::Profile::kImageRgba);
}
}  // namespace rhythm::image_shader
