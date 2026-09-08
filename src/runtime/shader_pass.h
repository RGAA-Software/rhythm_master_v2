#pragma once

#include "rhythm/runtime/runtime.h"

namespace rhythm::runtime::detail {
// Host-thread cache: immutable assets share programs across all nodes and views.
class ShaderPrograms final {
   public:
    void Retain(const graph::ExecutionPlan& plan, const image_shader::Resources& resources);
    render::DrawList Draw(const graph::Instruction& instruction,
                          std::span<const NodeOutput> outputs, double seconds,
                          render::TextureHandle fallback, render::Extent extent,
                          const image_shader::Resources& resources, render::Renderer& renderer);

   private:
    std::map<std::string, render::ImageProgram> programs_{};
};
}  // namespace rhythm::runtime::detail
