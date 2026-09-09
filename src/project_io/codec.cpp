#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>

#include <cmath>
#include <set>
#include <stdexcept>

#include "control_codec.h"
#include "feature_versions.h"
#include "graph.pb.h"
#include "graph_records.h"
#include "graph_validation.h"
#include "rhythm/graph/bindings.h"
#include "rhythm/project/store.h"
#include "wire_limits.h"
#include "wire_serialization.h"

namespace rhythm::project {
graph::Document DecodeGraph(std::string_view bytes) {
    if (bytes.size() > kMaximumGraphBytes) throw std::length_error("project.graph_bytes");
    detail::CheckWireLimits(bytes, detail::WireRoot::kGraph);
    schema::GraphProject message;
    google::protobuf::io::ArrayInputStream source(bytes.data(), static_cast<int>(bytes.size()));
    google::protobuf::io::CodedInputStream input(&source);
    input.SetRecursionLimit(32);
    input.SetTotalBytesLimit(static_cast<int>(kMaximumGraphBytes));
    if (!message.ParseFromCodedStream(&input) || !input.ConsumedEntireMessage() ||
        (message.schema_version() < 1 || message.schema_version() > 7))
        throw std::invalid_argument("project.graph_schema");
    if ((message.schema_version() >= 2) != message.has_canvas())
        throw std::invalid_argument("project.canvas_schema");
    if (message.schema_version() < 3 && (!message.signals().empty() || !message.bindings().empty()))
        throw std::invalid_argument("project.binding_schema");
    if (message.schema_version() < 4 && !message.components().empty())
        throw std::invalid_argument("project.component_schema");
    graph::Document document;
    if (message.schema_version() < 7 &&
        ((message.schema_version() == 6) != message.has_beat_grid()))
        throw std::invalid_argument("project.beat_schema");
    if (message.has_beat_grid()) document.beat_grid_ = detail::DecodeBeatGrid(message.beat_grid());
    document.id_ = message.id();
    document.revision_ = message.revision();
    document.output_ = message.output();
    detail::DecodeControls(message.controls(), document);
    if (message.schema_version() < 5 && !document.control_cues_.empty())
        throw std::invalid_argument("project.cue_schema");
    if (message.has_canvas())
        document.canvas_ = {message.canvas().width(), message.canvas().height()};
    for (const auto& record : message.nodes())
        document.nodes_.push_back(detail::DecodeNode(record));
    for (const auto& record : message.edges())
        document.edges_.push_back({record.id(), record.from(), record.to(), record.input(),
                                   record.SerializeAsString()});
    for (const auto& record : message.signals())
        document.signals_.push_back({record.name(), record.source(), record.SerializeAsString()});
    for (const auto& record : message.bindings())
        document.bindings_.push_back(
                {record.node(), record.input(), record.signal(), record.SerializeAsString()});
    for (const auto& record : message.components())
        document.components_.push_back(detail::DecodeComponent(record));
    if (message.schema_version() < 7 && detail::HasEvents(document))
        throw std::invalid_argument("project.event_schema");
    message.clear_components();
    // Each nested record retains its own unknown extensions.
    message.clear_signals();
    message.clear_bindings();
    message.clear_nodes();
    message.clear_edges();
    document.extensions_ = message.SerializeAsString();
    detail::ValidateGraph(document);
    return document;
}
std::string EncodeGraph(const graph::Document& document) {
    detail::ValidateGraph(document);
    schema::GraphProject message;
    if (!document.extensions_.empty() && !message.ParseFromString(document.extensions_))
        throw std::invalid_argument("project.extensions");
    message.set_schema_version(detail::HasEvents(document)                               ? 7
                               : document.beat_grid_                                     ? 6
                               : !document.control_cues_.empty()                         ? 5
                               : !document.components_.empty()                           ? 4
                               : document.signals_.empty() && document.bindings_.empty() ? 2
                                                                                         : 3);
    message.mutable_canvas()->set_width(document.canvas_.width_);
    message.mutable_canvas()->set_height(document.canvas_.height_);
    message.set_id(document.id_);
    message.set_revision(document.revision_);
    message.set_output(document.output_);
    if (document.beat_grid_)
        detail::EncodeBeatGrid(*document.beat_grid_, *message.mutable_beat_grid());
    else
        message.clear_beat_grid();
    message.clear_nodes();
    message.clear_edges();
    for (const auto& node : document.nodes_) detail::EncodeNode(node, *message.add_nodes());
    for (const auto& edge : document.edges_) {
        auto& record = *message.add_edges();
        if (!edge.extensions_.empty() && !record.ParseFromString(edge.extensions_))
            throw std::invalid_argument("project.extensions");
        record.set_id(edge.id_);
        record.set_from(edge.from_);
        record.set_to(edge.to_);
        record.set_input(edge.input_);
    }
    message.clear_signals();
    message.clear_bindings();
    for (const auto& signal : document.signals_) {
        auto& record = *message.add_signals();
        if (!signal.extensions_.empty() && !record.ParseFromString(signal.extensions_))
            throw std::invalid_argument("project.extensions");
        record.set_name(signal.name_);
        record.set_source(signal.source_);
    }
    for (const auto& binding : document.bindings_) {
        auto& record = *message.add_bindings();
        if (!binding.extensions_.empty() && !record.ParseFromString(binding.extensions_))
            throw std::invalid_argument("project.extensions");
        record.set_node(binding.node_);
        record.set_input(binding.input_);
        record.set_signal(binding.signal_);
    }
    message.clear_components();
    for (const auto& definition : document.components_)
        detail::EncodeComponent(definition, *message.add_components());
    if (!document.control_titles_.empty() || !document.control_snapshots_.empty() ||
        message.has_controls())
        detail::EncodeControls(document, *message.mutable_controls());
    if (message.ByteSizeLong() > kMaximumGraphBytes) throw std::length_error("project.graph_bytes");
    const auto bytes = detail::SerializeDeterministically(message);
    // Parse validates UTF-8 on the write boundary, including unknown node strings.
    DecodeGraph(bytes);
    return bytes;
}
}  // namespace rhythm::project
