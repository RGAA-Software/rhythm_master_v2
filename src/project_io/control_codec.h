#pragma once

#include "graph.pb.h"
#include "rhythm/graph/document.h"

namespace rhythm::project::detail {
void DecodeControls(const schema::ControlMetadata& record, graph::Document& document);
void EncodeControls(const graph::Document& document, schema::ControlMetadata& record);
}  // namespace rhythm::project::detail
