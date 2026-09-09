#include "surface_pass.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

namespace rhythm::runtime::detail {
void SurfacePrograms::Retain(const graph::ExecutionPlan& plan,
                             const surface_shader::Resources& resources) {
    std::set<std::string> required;
    std::size_t total = 0;
    for (const auto& instruction : plan.instructions_) {
        if (instruction.operation_ != graph::Operation::kMaterialShader) continue;
        const auto& id = std::get<assets::AssetId>(instruction.node_.properties_.at("asset"));
        if (!required.insert(id.sha256_).second) continue;
        const auto found = resources.programs_.find(id.sha256_);
        if (!assets::ValidId(id) || found == resources.programs_.end())
            throw std::invalid_argument("shader.asset_missing");
        if (required.size() > surface_shader::kMaximumPrograms)
            throw std::length_error("shader.program_budget");
        for (const auto& artifact : found->second.artifacts_) {
            if (artifact.size() > surface_shader::kMaximumProgramBytes - total)
                throw std::length_error("shader.program_budget");
            total += artifact.size();
        }
    }
    std::erase_if(programs_, [&](const auto& item) { return !required.contains(item.first); });
}
render::SurfaceProgramInput SurfacePrograms::Bind(const graph::Instruction& instruction,
                                                  std::span<const NodeOutput> outputs,
                                                  double seconds,
                                                  const surface_shader::Resources& resources,
                                                  render::Renderer& renderer) {
    const auto& node = instruction.node_;
    const auto& id = std::get<assets::AssetId>(node.properties_.at("asset"));
    auto& program = programs_[id.sha256_];
    if (!renderer.IsValid(program.Handle())) {
        const auto target =
                renderer.SurfaceTarget() == render::SurfaceProgramTarget::kWindowsSm5 ? 0u : 1u;
        program = renderer.CreateSurfaceProgram(
                resources.programs_.at(id.sha256_).artifacts_[static_cast<std::size_t>(target)]);
    }
    render::SurfaceProgramInput input;
    input.program_ = program.Handle();
    const auto value = [&](std::size_t port, std::string_view key, double initial) {
        const double result = instruction.inputs_[port]
                                      ? outputs[*instruction.inputs_[port]].scalar_
                                      : graph::Scalar(node, key, initial);
        return std::isfinite(result) ? result : initial;
    };
    input.seconds_ = static_cast<float>(std::clamp(value(1, "shader_time", seconds), 0.0, 1e6));
    constexpr std::array<std::string_view, 4> kParameters{"a", "b", "c", "d"};
    for (std::size_t index = 0; index < kParameters.size(); ++index)
        input.parameters_[index] =
                static_cast<float>(std::clamp(value(index + 2, kParameters[index], 0), -1e6, 1e6));
    return input;
}
}  // namespace rhythm::runtime::detail
