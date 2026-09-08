#include <algorithm>
#include <cmath>
#include <map>
#include <set>

#include "rhythm/graph/bindings.h"
#include "rhythm/graph/components.h"

namespace rhythm::graph {
namespace {
struct Descriptions {
    std::map<std::string, std::optional<OperatorDescriptor>> cache_{};
    std::set<std::string> active_{};
    std::size_t nodes_ = 0;
};
std::optional<OperatorDescriptor> Describe(std::string_view type, const Registry& registry,
                                           std::span<const ComponentDefinition> definitions,
                                           Descriptions& state) {
    if (const auto native = registry.Find(type)) return native;
    const std::string key(type);
    if (const auto cached = state.cache_.find(key); cached != state.cache_.end())
        return cached->second;
    if (!type.starts_with("component.") || state.active_.size() >= 16 ||
        state.active_.contains(key))
        return {};
    const auto found = std::find_if(definitions.begin(), definitions.end(),
                                    [&](const auto& value) { return value.type_ == type; });
    if (found == definitions.end() || found->version_ != 1 || found->nodes_.empty() ||
        found->inputs_.size() > 128 || found->parameters_.size() > 128)
        return {};
    state.nodes_ += found->nodes_.size();
    if (state.nodes_ > 10000) return {};
    state.active_.insert(key);
    std::map<NodeId, OperatorDescriptor> inner;
    std::map<NodeId, Node> configurations;
    OperatorDescriptor result;
    result.type_ = type;
    result.operation_ = Operation::kComponent;
    for (const auto& node : found->nodes_) {
        // Public controls belong to the work. Components receive typed inputs.
        if (node.type_ == "control.scalar") return {};
        const auto descriptor = Describe(node.type_, registry, definitions, state);
        if (!node.id_ || node.version_ != 1 || !descriptor ||
            !inner.emplace(node.id_, *descriptor).second)
            return {};
        configurations.emplace(node.id_, node);
        result.time_dependent_ |= descriptor->time_dependent_;
    }
    if (!inner.contains(found->output_)) return {};
    result.output_ = inner.at(found->output_).output_;
    std::set<std::string> keys;
    std::set<std::pair<NodeId, std::string>> targets;
    for (const auto& port : found->inputs_) {
        if (!ValidSignalName(port.key_) || !keys.insert(port.key_).second ||
            !targets.emplace(port.node_, port.input_).second || !inner.contains(port.node_))
            return {};
        const auto& inputs = inner.at(port.node_).inputs_;
        const auto input = std::find_if(inputs.begin(), inputs.end(), [&](const auto& value) {
            return value.key_ == port.input_;
        });
        if (input == inputs.end()) return {};
        auto exposed = *input;
        exposed.key_ = port.key_;
        if (std::any_of(found->edges_.begin(), found->edges_.end(),
                        [&](const auto& edge) {
                            return edge.to_ == port.node_ && edge.input_ == port.input_;
                        }) ||
            std::any_of(found->bindings_.begin(), found->bindings_.end(), [&](const auto& binding) {
                return binding.node_ == port.node_ && binding.input_ == port.input_;
            }))
            exposed.required_ = false;
        result.inputs_.push_back(std::move(exposed));
    }
    keys.clear();
    targets.clear();
    for (const auto& parameter : found->parameters_) {
        if (!ValidSignalName(parameter.key_) || parameter.group_.size() > 128 ||
            !keys.insert(parameter.key_).second ||
            !targets.emplace(parameter.node_, parameter.property_).second ||
            !inner.contains(parameter.node_))
            return {};
        const auto& properties = inner.at(parameter.node_).properties_;
        const auto property =
                std::find_if(properties.begin(), properties.end(),
                             [&](const auto& value) { return value.key_ == parameter.property_; });
        if (property == properties.end()) return {};
        auto exposed = *property;
        exposed.key_ = parameter.key_;
        exposed.group_ = parameter.group_;
        const auto& configured = configurations.at(parameter.node_).properties_;
        if (const auto value = configured.find(parameter.property_); value != configured.end()) {
            if (value->second.index() != exposed.default_.index()) return {};
            exposed.default_ = value->second;
        }
        if (parameter.minimum_ || parameter.maximum_) {
            if (!std::holds_alternative<double>(exposed.default_)) return {};
            const double minimum = parameter.minimum_.value_or(exposed.minimum_);
            const double maximum = parameter.maximum_.value_or(exposed.maximum_);
            if (!std::isfinite(minimum) || !std::isfinite(maximum) || minimum > maximum ||
                minimum < exposed.minimum_ || maximum > exposed.maximum_)
                return {};
            if ((exposed.integral_ || !exposed.choices_.empty()) &&
                (std::floor(minimum) != minimum || std::floor(maximum) != maximum))
                return {};
            exposed.minimum_ = minimum;
            exposed.maximum_ = maximum;
        }
        if (std::holds_alternative<double>(exposed.default_)) {
            const double value = std::get<double>(exposed.default_);
            if (!std::isfinite(value) || value < exposed.minimum_ || value > exposed.maximum_)
                return {};
        }
        result.properties_.push_back(std::move(exposed));
    }
    state.active_.erase(key);
    state.cache_[key] = result;
    return result;
}
}  // namespace
std::optional<OperatorDescriptor> DescribeComponent(
        std::string_view type, const Registry& registry,
        std::span<const ComponentDefinition> definitions) {
    if (definitions.size() > 256) return {};
    Descriptions state;
    return Describe(type, registry, definitions, state);
}
}  // namespace rhythm::graph
