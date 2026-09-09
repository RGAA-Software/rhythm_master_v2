#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "rhythm/assets/types.h"
#include "rhythm/parameters/beat_grid.h"
#include "rhythm/parameters/control_sequence.h"
#include "rhythm/parameters/controls.h"
#include "rhythm/parameters/curve.h"
#include "rhythm/parameters/expression.h"

namespace rhythm::graph {
using NodeId = std::uint64_t;
enum class ValueType : std::uint8_t {
    kScalar,
    kSignal,
    kTexture,
    kPoints,
    kGeometry,
    kMaterial,
    kScene,
    kCamera,
    kGpuPoints,
    kSceneImage,
    kDepth,
    kPath,
    kEvent
};
struct Canvas {
    std::uint32_t width_ = 640;
    std::uint32_t height_ = 360;
    bool operator==(const Canvas&) const = default;
};
inline bool ValidCanvas(Canvas canvas) {
    return canvas.width_ >= 16 && canvas.height_ >= 16 && canvas.width_ <= 4096 &&
           canvas.height_ <= 4096 && std::uint64_t(canvas.width_) * canvas.height_ <= 2073600;
}
struct Color {
    double r_ = 0;
    double g_ = 0;
    double b_ = 0;
    double a_ = 1;
    bool operator==(const Color&) const = default;
};
struct UnknownProperty {
    std::string encoded_{};
    bool operator==(const UnknownProperty&) const = default;
};
using Property = std::variant<double, Color, parameters::Curve, UnknownProperty,
                              parameters::Expression, assets::AssetId>;
struct Node {
    NodeId id_ = 0;
    std::string type_{};
    std::uint32_t version_ = 1;
    std::map<std::string, Property> properties_{};
    // Opaque codec-owned extension data permits unknown fields to round-trip.
    std::string extensions_{};
    bool operator==(const Node&) const = default;
};
struct Edge {
    std::uint64_t id_ = 0;
    NodeId from_ = 0;
    NodeId to_ = 0;
    std::string input_{};
    std::string extensions_{};
    bool operator==(const Edge&) const = default;
};
struct NamedSignal {
    std::string name_{};
    NodeId source_ = 0;
    std::string extensions_{};
    bool operator==(const NamedSignal&) const = default;
};
struct SignalBinding {
    NodeId node_ = 0;
    std::string input_{};
    std::string signal_{};
    std::string extensions_{};
    bool operator==(const SignalBinding&) const = default;
};
struct ComponentInput {
    std::string key_{};
    NodeId node_ = 0;
    std::string input_{};
    std::string extensions_{};
    bool operator==(const ComponentInput&) const = default;
};
struct ComponentParameter {
    std::string key_{};
    NodeId node_ = 0;
    std::string property_{};
    std::string group_{};
    std::string extensions_{};
    std::optional<double> minimum_{};
    std::optional<double> maximum_{};
    bool operator==(const ComponentParameter&) const = default;
};
// Embedded definitions share one project library. Nested instances reference
// another definition's stable type key; bodies do not own recursive pointers.
struct ComponentDefinition {
    std::string type_{};
    std::uint32_t version_ = 1;
    std::vector<Node> nodes_{};
    std::vector<Edge> edges_{};
    NodeId output_ = 0;
    std::vector<NamedSignal> signals_{};
    std::vector<SignalBinding> bindings_{};
    std::vector<ComponentInput> inputs_{};
    std::vector<ComponentParameter> parameters_{};
    std::string extensions_{};
    std::string title_{};
    bool operator==(const ComponentDefinition&) const = default;
};
struct Document {
    std::string id_{};
    std::uint64_t revision_ = 0;
    std::vector<Node> nodes_{};
    std::vector<Edge> edges_{};
    NodeId output_ = 0;
    std::string extensions_{};
    Canvas canvas_{};
    std::vector<NamedSignal> signals_{};
    std::vector<SignalBinding> bindings_{};
    std::vector<ComponentDefinition> components_{};
    std::map<NodeId, std::string> control_titles_{};
    std::vector<parameters::ControlSnapshot> control_snapshots_{};
    std::vector<parameters::ControlCue> control_cues_{};
    std::optional<parameters::BeatSettings> beat_grid_{};
    bool operator==(const Document&) const = default;
};
struct Diagnostic {
    std::string code_{};
    NodeId node_ = 0;
    std::string field_{};
};
}  // namespace rhythm::graph
