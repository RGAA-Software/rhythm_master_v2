#pragma once

#include "rhythm/runtime/runtime.h"

namespace rhythm::runtime::detail {
void EvaluateScene(const graph::Instruction& instruction, std::span<const NodeOutput> inputs,
                   NodeOutput& output, const scene::Resources& resources);
std::vector<graph::GeometryBudget> GeometryBudgets(const graph::ExecutionPlan& plan,
                                                   const scene::Resources& resources);
scene::Scene PreviewScene(const NodeOutput& output);
}  // namespace rhythm::runtime::detail
