#pragma once

#include "rhythm/graph/registry.h"

namespace rhythm::graph {
struct Instruction {
    Node node_{};
    Operation operation_ = Operation::kTime;
    std::vector<std::optional<std::size_t>> inputs_{};
};
// Published as a value and consumed through const references. Inputs index this
// plan, never addresses in an editor or registry. Feedback writes after evaluation.
struct ExecutionPlan {
    std::string document_id_{};
    std::uint64_t revision_ = 0;
    std::vector<Instruction> instructions_{};
    std::size_t output_ = 0;
    Canvas canvas_{};
};
using CompileResult = std::variant<ExecutionPlan, std::vector<Diagnostic>>;
// Shared initial CPU-point profile; checked during compilation/publication and
// before runtime allocation. Counts capacity, including intermediate snapshots.
std::optional<Diagnostic> ValidatePointBudget(const ExecutionPlan& plan);
// Initial procedural scene profile: bounded unique geometry and instance copies.
struct GeometryBudget {
    NodeId node_ = 0;
    std::uint64_t vertices_ = 0;
    std::uint64_t indices_ = 0;
    std::uint64_t draw_indices_ = 0;
    std::uint64_t draws_ = 0;
};
// Source compilation can defer imported geometry counts. Resource preparation
// and execution must supply the resolved counts (including an empty span).
std::optional<Diagnostic> ValidateSceneBudget(
        const ExecutionPlan& plan,
        std::optional<std::span<const GeometryBudget>> geometry = std::nullopt);
CompileResult Compile(const Document& document, const Registry& registry,
                      std::span<const NodeId> viewers = {});
}  // namespace rhythm::graph
