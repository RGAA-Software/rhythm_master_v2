#pragma once

#include "graph.pb.h"
#include "rhythm/graph/document.h"

namespace rhythm::project::detail {
graph::Node DecodeNode(const schema::Node& record);
void EncodeNode(const graph::Node& node, schema::Node& record);
graph::ComponentDefinition DecodeComponent(const schema::Component& record);
void EncodeComponent(const graph::ComponentDefinition& definition, schema::Component& record);
}  // namespace rhythm::project::detail
