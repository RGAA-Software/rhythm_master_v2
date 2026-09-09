#pragma once

#include "graph.pb.h"
#include "rhythm/graph/document.h"

namespace rhythm::project::detail {
parameters::BeatSettings DecodeBeatGrid(const schema::BeatGrid& record);
void EncodeBeatGrid(const parameters::BeatSettings& settings, schema::BeatGrid& record);
void DecodeControls(const schema::ControlMetadata& record, graph::Document& document);
void EncodeControls(const graph::Document& document, schema::ControlMetadata& record);
}  // namespace rhythm::project::detail
