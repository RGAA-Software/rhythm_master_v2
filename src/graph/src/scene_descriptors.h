#pragma once
#include "rhythm/graph/registry.h"
namespace rhythm::graph {
void AppendPathDescriptors(std::vector<OperatorDescriptor>& operators);
void AppendSceneDescriptors(std::vector<OperatorDescriptor>& operators);
}  // namespace rhythm::graph
