#include <algorithm>
#include <iostream>
#include <stdexcept>

#include "rhythm/graph/compiler.h"
#include "rhythm/graph/components.h"

namespace {
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
void Run() {
    using namespace rhythm::graph;
    Registry registry;
    ComponentDefinition color;
    color.type_ = "component.test.color";
    color.nodes_ = {registry.MakeNode(1, "scalar.constant"),
                    registry.MakeNode(2, "texture.gradient")};
    color.nodes_[0].properties_["value"] = 0.25;
    color.output_ = 2;
    color.edges_ = {{1, 1, 2, "amount"}};
    color.inputs_ = {{"amount", 2, "amount"}};
    color.parameters_ = {{"value", 1, "value", "Color"}, {"color_a", 2, "color_a", "Color"}};
    color.parameters_[0].minimum_ = 0;
    color.parameters_[0].maximum_ = 1;
    Document document;
    document.id_ = "component.test";
    document.components_ = {color};
    document.nodes_ = {registry.MakeNode(10, color.type_, document.components_),
                       registry.MakeNode(20, color.type_, document.components_),
                       registry.MakeNode(30, "texture.blend"),
                       registry.MakeNode(40, "output.texture")};
    document.nodes_[1].properties_["value"] = 0.75;
    document.edges_ = {{1, 10, 30, "a"}, {2, 20, 30, "b"}, {3, 30, 40, "source"}};
    document.output_ = 40;
    const auto descriptor = registry.Find(color.type_, document.components_);
    Require(descriptor && descriptor->output_ == ValueType::kTexture &&
                    descriptor->inputs_[0].type_ == ValueType::kScalar,
            "component public types");
    Require(descriptor->properties_[0].minimum_ == 0 && descriptor->properties_[0].maximum_ == 1,
            "curated public bounds");
    const auto plan = std::get<ExecutionPlan>(Compile(document, registry));
    Require(plan.instructions_.size() == 6, "instance expansion count");
    std::vector<double> values;
    for (const auto& instruction : plan.instructions_) {
        Require(instruction.operation_ != Operation::kComponent,
                "no component operation reaches runtime");
        if (instruction.operation_ == Operation::kConstant)
            values.push_back(Scalar(instruction.node_, "value", 0));
    }
    std::sort(values.begin(), values.end());
    Require(values == std::vector<double>{0.25, 0.75}, "independent public parameter overrides");
    const auto expanded = std::get<Document>(ExpandComponents(document, registry));
    Require(expanded.components_.empty() && expanded.bindings_.empty(), "flat contract");
    for (NodeId id : {10, 20})
        Require(std::any_of(expanded.nodes_.begin(), expanded.nodes_.end(),
                            [&](const auto& node) {
                                return node.id_ == id && node.type_ == "texture.gradient";
                            }),
                "stable instance viewer identity");
    document.nodes_.push_back(registry.MakeNode(50, "scalar.constant"));
    document.nodes_.back().properties_["value"] = 0.9;
    document.edges_.push_back({4, 50, 10, "amount"});
    const auto overridden = std::get<ExecutionPlan>(Compile(document, registry));
    Require(overridden.instructions_.size() == 6,
            "external input replaces internal default dependency");
    ComponentDefinition nested;
    nested.type_ = "component.test.nested";
    nested.nodes_ = {registry.MakeNode(1, color.type_, document.components_)};
    nested.output_ = 1;
    nested.inputs_ = {{"amount", 1, "amount"}};
    nested.parameters_ = {{"value", 1, "value", "Nested"}};
    document.components_.push_back(nested);
    document.nodes_[1] = registry.MakeNode(20, nested.type_, document.components_);
    document.nodes_[1].properties_["value"] = 0.6;
    Require(std::holds_alternative<ExecutionPlan>(Compile(document, registry)),
            "nested component compiles");
    auto cyclic = document;
    cyclic.components_[1].nodes_[0].type_ = nested.type_;
    Require(std::holds_alternative<std::vector<Diagnostic>>(Compile(cyclic, registry)),
            "recursive component rejects");
    auto invalid = document;
    invalid.components_[0].parameters_[0].property_ = "missing";
    Require(std::holds_alternative<std::vector<Diagnostic>>(Compile(invalid, registry)),
            "invalid exposed parameter rejects");
    invalid = document;
    invalid.nodes_[1].properties_["value"] = Color{};
    Require(std::holds_alternative<std::vector<Diagnostic>>(Compile(invalid, registry)),
            "invalid instance parameter rejects");
    std::cout << "component expansion: types, isolated parameters, stable IDs, input overrides, "
                 "nesting and invalid-recursion rejection passed\n";
}
}  // namespace
int main() {
    try {
        Run();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
