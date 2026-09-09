#include "rhythm/editor/transform_drivers.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

#include "rhythm/graph/bindings.h"

namespace rhythm::editor {
namespace {
const graph::Node& FindNode(const graph::Document& document, graph::NodeId id) {
    const auto found = std::find_if(document.nodes_.begin(), document.nodes_.end(),
                                    [&](const auto& node) { return node.id_ == id; });
    if (found == document.nodes_.end()) throw std::invalid_argument("graph.missing_node");
    return *found;
}
std::vector<TransformDriver> Drivers(const graph::Document& document,
                                     const graph::Registry& registry, graph::NodeId selected) {
    const auto& node = FindNode(document, selected);
    if (node.type_ != "texture.affine" && node.type_ != "scene.transform")
        throw std::invalid_argument("transform_driver.select_transform");
    const auto descriptor = registry.Find(node.type_);
    const auto resolved = graph::ResolveEdges(document);
    if (std::holds_alternative<std::vector<graph::Diagnostic>>(resolved))
        throw std::invalid_argument("transform_driver.invalid_sources");
    const auto& edges = std::get<std::vector<graph::Edge>>(resolved);
    std::vector<TransformDriver> result;
    for (const auto& port : descriptor->inputs_) {
        if (port.type_ != graph::ValueType::kScalar) continue;
        const auto property =
                std::find_if(descriptor->properties_.begin(), descriptor->properties_.end(),
                             [&](const auto& value) { return value.key_ == port.key_; });
        if (property == descriptor->properties_.end())
            throw std::invalid_argument("transform_driver.invalid_sources");
        for (const auto& edge : edges) {
            if (edge.to_ != selected || edge.input_ != port.key_) continue;
            if (std::any_of(result.begin(), result.end(),
                            [&](const auto& driver) { return driver.property_ == port.key_; }))
                throw std::invalid_argument("transform_driver.invalid_sources");
            const auto& source = FindNode(document, edge.from_);
            const auto source_descriptor = registry.Find(source.type_, document.components_);
            if (!source_descriptor || source_descriptor->output_ != graph::ValueType::kScalar)
                throw std::invalid_argument("transform_driver.invalid_sources");
            TransformDriver driver{port.key_, source.id_,         source.type_,      {},
                                   {},        property->minimum_, property->maximum_};
            for (const auto& binding : document.bindings_)
                if (binding.node_ == selected && binding.input_ == port.key_)
                    driver.signal_ = binding.signal_;
            if (source.type_ == "scalar.curve")
                for (const auto& clock : edges)
                    if (clock.to_ == source.id_ && clock.input_ == "time") {
                        if (driver.curve_clock_)
                            throw std::invalid_argument("transform_driver.invalid_sources");
                        driver.curve_clock_ = clock.from_;
                    }
            result.push_back(std::move(driver));
        }
    }
    return result;
}
void Match(const Snapshot& snapshot, const TransformSample& sample) {
    if (snapshot.document_.id_ != sample.document_id_ ||
        snapshot.document_.revision_ != sample.revision_)
        throw std::invalid_argument("transform_driver.stale_frame");
}
double Observed(const TransformSample& sample, graph::NodeId id) {
    const auto found = sample.scalars_.find(id);
    if (found == sample.scalars_.end() || !std::isfinite(found->second))
        throw std::invalid_argument("transform_driver.missing_value");
    return found->second;
}
}  // namespace
TransformDrivers InspectTransformDrivers(const graph::Document& document,
                                         const graph::Registry& registry, graph::NodeId selected) {
    try {
        return Drivers(document, registry, selected);
    } catch (const std::exception& error) {
        return graph::Diagnostic{error.what(), selected};
    }
}
EditResult FreezeTransformDrivers(const Snapshot& snapshot, const graph::Registry& registry,
                                  graph::NodeId selected, const TransformSample& sample) {
    try {
        Match(snapshot, sample);
        const auto drivers = Drivers(snapshot.document_, registry, selected);
        auto next = snapshot;
        std::set<std::string> properties;
        auto node = std::find_if(next.document_.nodes_.begin(), next.document_.nodes_.end(),
                                 [&](const auto& value) { return value.id_ == selected; });
        for (const auto& driver : drivers) {
            node->properties_[driver.property_] =
                    std::clamp(Observed(sample, driver.source_), driver.minimum_, driver.maximum_);
            properties.insert(driver.property_);
        }
        std::erase_if(next.document_.edges_, [&](const auto& edge) {
            return edge.to_ == selected && properties.contains(edge.input_);
        });
        std::erase_if(next.document_.bindings_, [&](const auto& binding) {
            return binding.node_ == selected && properties.contains(binding.input_);
        });
        return next;
    } catch (const std::exception& error) {
        return graph::Diagnostic{error.what(), selected};
    }
}
EditResult RecordTransformKey(const Snapshot& snapshot, const graph::Registry& registry,
                              graph::NodeId selected, const std::string& property, double value,
                              const TransformSample& sample) {
    try {
        Match(snapshot, sample);
        const auto drivers = Drivers(snapshot.document_, registry, selected);
        const auto driver = std::find_if(drivers.begin(), drivers.end(), [&](const auto& item) {
            return item.property_ == property;
        });
        if (driver == drivers.end() || !driver->curve_clock_ ||
            driver->source_type_ != "scalar.curve")
            throw std::invalid_argument("transform_driver.not_curve");
        if (!std::isfinite(value) || value < driver->minimum_ || value > driver->maximum_)
            throw std::invalid_argument("transform_driver.range");
        (void)Observed(sample, driver->source_);
        const auto seconds = Observed(sample, *driver->curve_clock_);
        if (seconds < 0 || seconds > 1e9)
            throw std::invalid_argument("transform_driver.curve_time");
        auto next = snapshot;
        auto source = std::find_if(next.document_.nodes_.begin(), next.document_.nodes_.end(),
                                   [&](const auto& node) { return node.id_ == driver->source_; });
        auto curve = source->properties_.contains("curve")
                             ? std::get<parameters::Curve>(source->properties_.at("curve"))
                             : parameters::Curve{};
        std::vector<parameters::Keyframe> keys(curve.Keys().begin(), curve.Keys().end());
        auto existing = std::find_if(keys.begin(), keys.end(), [&](const auto& key) {
            return std::abs(key.seconds_ - seconds) <= 1e-9;
        });
        if (existing != keys.end())
            existing->value_ = value;
        else {
            keys.push_back({seconds, value});
            std::sort(keys.begin(), keys.end(),
                      [](const auto& a, const auto& b) { return a.seconds_ < b.seconds_; });
        }
        curve.SetKeys(std::move(keys));
        source->properties_["curve"] = std::move(curve);
        return next;
    } catch (const std::exception& error) {
        return graph::Diagnostic{error.what(), selected, property};
    }
}
}  // namespace rhythm::editor
