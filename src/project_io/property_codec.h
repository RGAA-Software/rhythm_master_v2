#pragma once

#include "graph.pb.h"
#include "rhythm/graph/document.h"

namespace rhythm::project::detail {
graph::Property DecodeProperty(const schema::Property& property, bool preserve_unknown);
void EncodeProperty(const graph::Property& value, schema::Property& property);
}  // namespace rhythm::project::detail
