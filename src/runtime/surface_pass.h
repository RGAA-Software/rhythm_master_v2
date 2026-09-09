#pragma once

#include "rhythm/runtime/runtime.h"

namespace rhythm::runtime::detail {
// Immutable surface assets share render-thread programs across nodes and views.
class SurfacePrograms final {
   public:
    void Retain(const graph::ExecutionPlan& plan, const surface_shader::Resources& resources);
    render::SurfaceProgramInput Bind(const graph::Instruction& instruction,
                                     std::span<const NodeOutput> outputs, double seconds,
                                     const surface_shader::Resources& resources,
                                     render::Renderer& renderer);

   private:
    std::map<std::string, render::SurfaceProgram> programs_{};
};
}  // namespace rhythm::runtime::detail
