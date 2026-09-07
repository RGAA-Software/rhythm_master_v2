#include <cmath>

#include "rhythm/graph/compiler.h"

namespace rhythm::graph {
std::optional<Diagnostic> ValidatePointBudget(const ExecutionPlan& plan) {
    if (plan.instructions_.size() > 10000) return Diagnostic{"graph.limit"};
    std::vector<std::uint32_t> counts(plan.instructions_.size());
    std::uint64_t total = 0;
    std::uint64_t physics_total = 0;
    for (std::size_t index = 0; index < plan.instructions_.size(); ++index) {
        const auto& instruction = plan.instructions_[index];
        double count = 0;
        if (instruction.operation_ == Operation::kParticleEmitter)
            count = Scalar(instruction.node_, "particle_capacity", 2048);
        else if (instruction.operation_ == Operation::kPointGrid)
            count = Scalar(instruction.node_, "columns", 16) * Scalar(instruction.node_, "rows", 9);
        else if (instruction.operation_ == Operation::kPointTransform ||
                 instruction.operation_ == Operation::kPointPhysics) {
            if (instruction.inputs_.empty() || !instruction.inputs_[0] ||
                *instruction.inputs_[0] >= index)
                return Diagnostic{"graph.points_budget", instruction.node_.id_};
            count = counts[*instruction.inputs_[0]];
            if (instruction.operation_ == Operation::kPointPhysics) {
                physics_total += static_cast<std::uint64_t>(count);
                if (count > 512 || physics_total > 2048)
                    return Diagnostic{"graph.physics_budget", instruction.node_.id_};
            }
        }
        if (!std::isfinite(count) || count < 0 || count > 16384)
            return Diagnostic{"graph.points_budget", instruction.node_.id_};
        counts[index] = static_cast<std::uint32_t>(count);
        total += counts[index];
        if (total > 131072) return Diagnostic{"graph.points_budget", instruction.node_.id_};
    }
    return {};
}
}  // namespace rhythm::graph
