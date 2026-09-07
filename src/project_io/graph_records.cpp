#include "graph_records.h"

#include <stdexcept>

#include "property_codec.h"
#include "wire_serialization.h"

namespace rhythm::project::detail {
graph::Node DecodeNode(const schema::Node& record) {
    graph::Node node;
    node.id_ = record.id();
    node.type_ = record.type_key();
    node.version_ = record.schema_version();
    for (const auto& [key, value] : record.properties())
        node.properties_[key] = DecodeProperty(value, true);
    node.extensions_ = SerializeDeterministically(record);
    return node;
}
void EncodeNode(const graph::Node& node, schema::Node& record) {
    if (!node.extensions_.empty() && !record.ParseFromString(node.extensions_))
        throw std::invalid_argument("project.extensions");
    record.set_id(node.id_);
    record.set_type_key(node.type_);
    record.set_schema_version(node.version_);
    auto original = record.properties();
    record.clear_properties();
    for (const auto& [key, value] : node.properties_) {
        auto& property = (*record.mutable_properties())[key];
        if (original.contains(key)) property = original.at(key);
        EncodeProperty(value, property);
    }
}
graph::ComponentDefinition DecodeComponent(const schema::Component& record) {
    graph::ComponentDefinition result;
    result.type_ = record.type_key();
    result.title_ = record.title();
    result.version_ = record.schema_version();
    result.output_ = record.output();
    for (const auto& node : record.nodes()) result.nodes_.push_back(DecodeNode(node));
    for (const auto& edge : record.edges())
        result.edges_.push_back(
                {edge.id(), edge.from(), edge.to(), edge.input(), edge.SerializeAsString()});
    for (const auto& signal : record.signals())
        result.signals_.push_back({signal.name(), signal.source(), signal.SerializeAsString()});
    for (const auto& binding : record.bindings())
        result.bindings_.push_back(
                {binding.node(), binding.input(), binding.signal(), binding.SerializeAsString()});
    for (const auto& port : record.inputs())
        result.inputs_.push_back({port.key(), port.node(), port.input(), port.SerializeAsString()});
    for (const auto& parameter : record.parameters()) {
        result.parameters_.push_back({parameter.key(), parameter.node(), parameter.property(),
                                      parameter.group(), parameter.SerializeAsString()});
        if (parameter.has_minimum()) result.parameters_.back().minimum_ = parameter.minimum();
        if (parameter.has_maximum()) result.parameters_.back().maximum_ = parameter.maximum();
    }
    auto extensions = record;
    extensions.clear_nodes();
    extensions.clear_edges();
    extensions.clear_signals();
    extensions.clear_bindings();
    extensions.clear_inputs();
    extensions.clear_parameters();
    result.extensions_ = extensions.SerializeAsString();
    return result;
}
void EncodeComponent(const graph::ComponentDefinition& definition, schema::Component& record) {
    if (!definition.extensions_.empty() && !record.ParseFromString(definition.extensions_))
        throw std::invalid_argument("project.extensions");
    record.set_type_key(definition.type_);
    record.set_title(definition.title_);
    record.set_schema_version(definition.version_);
    record.set_output(definition.output_);
    record.clear_nodes();
    record.clear_edges();
    record.clear_signals();
    record.clear_bindings();
    record.clear_inputs();
    record.clear_parameters();
    for (const auto& node : definition.nodes_) EncodeNode(node, *record.add_nodes());
    for (const auto& edge : definition.edges_) {
        auto& value = *record.add_edges();
        if (!edge.extensions_.empty() && !value.ParseFromString(edge.extensions_))
            throw std::invalid_argument("project.extensions");
        value.set_id(edge.id_);
        value.set_from(edge.from_);
        value.set_to(edge.to_);
        value.set_input(edge.input_);
    }
    for (const auto& signal : definition.signals_) {
        auto& value = *record.add_signals();
        if (!signal.extensions_.empty() && !value.ParseFromString(signal.extensions_))
            throw std::invalid_argument("project.extensions");
        value.set_name(signal.name_);
        value.set_source(signal.source_);
    }
    for (const auto& binding : definition.bindings_) {
        auto& value = *record.add_bindings();
        if (!binding.extensions_.empty() && !value.ParseFromString(binding.extensions_))
            throw std::invalid_argument("project.extensions");
        value.set_node(binding.node_);
        value.set_input(binding.input_);
        value.set_signal(binding.signal_);
    }
    for (const auto& port : definition.inputs_) {
        auto& value = *record.add_inputs();
        if (!port.extensions_.empty() && !value.ParseFromString(port.extensions_))
            throw std::invalid_argument("project.extensions");
        value.set_key(port.key_);
        value.set_node(port.node_);
        value.set_input(port.input_);
    }
    for (const auto& parameter : definition.parameters_) {
        auto& value = *record.add_parameters();
        if (!parameter.extensions_.empty() && !value.ParseFromString(parameter.extensions_))
            throw std::invalid_argument("project.extensions");
        value.set_key(parameter.key_);
        value.set_node(parameter.node_);
        value.set_property(parameter.property_);
        value.set_group(parameter.group_);
        value.clear_minimum();
        value.clear_maximum();
        if (parameter.minimum_) value.set_minimum(*parameter.minimum_);
        if (parameter.maximum_) value.set_maximum(*parameter.maximum_);
    }
}
}  // namespace rhythm::project::detail
