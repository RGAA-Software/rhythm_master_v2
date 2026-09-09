#include <iostream>
#include <stdexcept>

#include "rhythm/graph/compiler.h"

namespace {
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void Run() {
    using namespace rhythm::graph;
    Registry registry;
    Document document;
    document.id_ = "scene.contract";
    document.nodes_ = {
            registry.MakeNode(1, "geometry.sphere"), registry.MakeNode(2, "material.unlit"),
            registry.MakeNode(3, "scene.instance"),  registry.MakeNode(4, "scene.transform"),
            registry.MakeNode(5, "scene.camera"),    registry.MakeNode(6, "scene.render"),
            registry.MakeNode(7, "output.texture")};
    document.edges_ = {{1, 1, 3, "geometry"}, {2, 2, 3, "material"}, {3, 3, 4, "scene"},
                       {4, 4, 6, "scene"},    {5, 5, 6, "camera"},   {6, 6, 7, "source"}};
    document.output_ = 7;
    Require(std::holds_alternative<ExecutionPlan>(Compile(document, registry)),
            "typed scene compiles");
    {
        auto sampled = document;
        sampled.nodes_[5].properties_["scene_antialiasing"] = 1.0;
        Require(std::holds_alternative<ExecutionPlan>(Compile(sampled, registry)),
                "explicit 2x scene color sampling compiles");
        sampled.nodes_[5].properties_["scene_antialiasing"] = 0.5;
        Require(std::holds_alternative<std::vector<Diagnostic>>(Compile(sampled, registry)),
                "fractional sampling profile rejected");
        sampled.nodes_[5].properties_["scene_antialiasing"] = 2.0;
        Require(std::holds_alternative<std::vector<Diagnostic>>(Compile(sampled, registry)),
                "unsupported sampling profile rejected");
        sampled.nodes_[5].properties_.erase("scene_antialiasing");
        Require(std::holds_alternative<ExecutionPlan>(Compile(sampled, registry)),
                "old scene render without explicit sampling remains valid");
    }
    {
        auto captured = document;
        captured.nodes_[5] = registry.MakeNode(6, "scene.capture");
        captured.nodes_.push_back(registry.MakeNode(8, "scene.color"));
        captured.nodes_.push_back(registry.MakeNode(9, "scene.depth"));
        captured.nodes_.push_back(registry.MakeNode(10, "texture.dof"));
        captured.edges_.back().from_ = 10;
        captured.edges_.insert(captured.edges_.end(), {{7, 6, 8, "capture"},
                                                       {8, 6, 9, "capture"},
                                                       {9, 8, 10, "source"},
                                                       {10, 9, 10, "depth"}});
        Require(std::holds_alternative<ExecutionPlan>(Compile(captured, registry)),
                "explicit capture color/depth feed depth of field");
        captured.edges_.back().from_ = 8;
        Require(std::holds_alternative<std::vector<Diagnostic>>(Compile(captured, registry)),
                "ordinary color cannot masquerade as depth");
    }
    auto bad = document;
    {
        Document shadow;
        shadow.id_ = "scene.shadow";
        shadow.nodes_ = {registry.MakeNode(1, "scene.directional_light"),
                         registry.MakeNode(2, "scene.shadow"), registry.MakeNode(3, "scene.render"),
                         registry.MakeNode(4, "output.texture")};
        shadow.edges_ = {{1, 1, 2, "scene"}, {2, 2, 3, "scene"}, {3, 3, 4, "source"}};
        shadow.output_ = 4;
        Require(std::holds_alternative<ExecutionPlan>(Compile(shadow, registry)),
                "shadow node compiles");
        shadow.nodes_[1].properties_["shadow_light"] = 1.0;
        Require(std::holds_alternative<std::vector<Diagnostic>>(Compile(shadow, registry)),
                "absent shadow light rejected before rendering");
        shadow.nodes_[1].properties_["shadow_light"] = 0.0;
        shadow.nodes_.push_back(registry.MakeNode(5, "scene.merge"));
        shadow.edges_[1].from_ = 5;
        shadow.edges_.push_back({4, 2, 5, "a"});
        shadow.edges_.push_back({5, 2, 5, "b"});
        Require(std::holds_alternative<std::vector<Diagnostic>>(Compile(shadow, registry)),
                "merging two shadow configurations rejects ambiguity");
    }
    bad.edges_[1].from_ = 1;
    Require(std::holds_alternative<std::vector<Diagnostic>>(Compile(bad, registry)),
            "geometry cannot connect to material input");
    for (NodeId id = 8; id <= 18; ++id) {
        const NodeId previous = id == 8 ? 4 : id - 1;
        document.nodes_.push_back(registry.MakeNode(id, "scene.merge"));
        document.edges_.push_back({id * 2, previous, id, "a"});
        document.edges_.push_back({id * 2 + 1, previous, id, "b"});
    }
    document.edges_[3].from_ = 18;
    const auto rejected = Compile(document, registry);
    Require(std::holds_alternative<std::vector<Diagnostic>>(rejected) &&
                    std::get<std::vector<Diagnostic>>(rejected).at(0).code_ == "graph.scene_budget",
            "exponential merge rejects before runtime allocation");
    ExecutionPlan malformed;
    malformed.instructions_.push_back(
            {registry.MakeNode(1, "scene.transform"), Operation::kSceneTransform, {0}});
    Require(ValidateSceneBudget(malformed).has_value(),
            "future/self input rejects in budget guard");
    ExecutionPlan lights;
    lights.instructions_.push_back(
            {registry.MakeNode(1, "scene.directional_light"), Operation::kDirectionalLight, {{}}});
    for (std::size_t i = 1; i <= 3; ++i)
        lights.instructions_.push_back(
                {registry.MakeNode(i + 1, "scene.merge"), Operation::kSceneMerge, {i - 1, i - 1}});
    Require(ValidateSceneBudget(lights).has_value(),
            "more than four lights rejects before rendering");
    for (const auto type : {"scene.point_light", "scene.spot_light"}) {
        auto mixed = document;
        mixed.nodes_.push_back(registry.MakeNode(100, type));
        mixed.nodes_.push_back(registry.MakeNode(101, "scene.merge"));
        mixed.edges_[3].from_ = 101;
        mixed.edges_.push_back({100, 4, 101, "a"});
        mixed.edges_.push_back({101, 100, 101, "b"});
        Require(std::holds_alternative<ExecutionPlan>(Compile(mixed, registry)),
                "positional scene light compiles");
    }
    Document instances;
    instances.id_ = "point.instances";
    instances.nodes_ = {registry.MakeNode(1, "geometry.cube"), registry.MakeNode(2, "point.grid"),
                        registry.MakeNode(3, "scene.point_instances"),
                        registry.MakeNode(4, "scene.render"),
                        registry.MakeNode(5, "output.texture")};
    instances.nodes_[1].properties_["columns"] = 100.0;
    instances.nodes_[1].properties_["rows"] = 100.0;
    instances.nodes_[2].properties_["instance_limit"] = 10000.0;
    instances.edges_ = {
            {1, 1, 3, "geometry"}, {2, 2, 3, "points"}, {3, 3, 4, "scene"}, {4, 4, 5, "source"}};
    instances.output_ = 5;
    Require(std::holds_alternative<ExecutionPlan>(Compile(instances, registry)),
            "ten thousand low-poly instances fit the bounded batch profile");
    instances.nodes_[0] = registry.MakeNode(1, "geometry.sphere");
    Require(std::holds_alternative<std::vector<Diagnostic>>(Compile(instances, registry)),
            "instancing retains rasterized triangle admission limits");
    std::cout << "Scene graph: typed ports, compilation and bounded merge passed\n";
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
