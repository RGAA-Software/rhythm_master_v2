#pragma once

#include "rhythm/runtime/runtime.h"

namespace rhythm::runtime::detail {
std::shared_ptr<const scene::Scene> PointInstances(const graph::Instruction& instruction,
                                                   std::span<const NodeOutput> outputs,
                                                   const ExternalInputs& external);
void EvaluatePath(const graph::Instruction& instruction, std::span<const NodeOutput> inputs,
                  NodeOutput& output);
scene::Scene PreviewPath(const NodeOutput& output);
void EvaluateScene(const graph::Instruction& instruction, std::span<const NodeOutput> inputs,
                   NodeOutput& output, const scene::Resources& resources);
std::vector<graph::GeometryBudget> GeometryBudgets(const graph::ExecutionPlan& plan,
                                                   const scene::Resources& resources);
scene::Scene PreviewScene(const NodeOutput& output);
}  // namespace rhythm::runtime::detail
