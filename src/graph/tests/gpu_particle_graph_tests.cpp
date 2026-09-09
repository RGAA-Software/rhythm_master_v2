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
    Document doc;
    doc.id_ = "gpu.particle.contract";
    doc.nodes_ = {registry.MakeNode(1, "gpu.particles"), registry.MakeNode(2, "gpu.render"),
                  registry.MakeNode(3, "output.texture")};
    doc.edges_ = {{1, 1, 2, "points"}, {2, 2, 3, "source"}};
    doc.output_ = 3;
    Require(std::holds_alternative<ExecutionPlan>(Compile(doc, registry)), "GPU graph compiles");
    doc.nodes_[1] = registry.MakeNode(2, "point.render");
    Require(std::holds_alternative<std::vector<Diagnostic>>(Compile(doc, registry)),
            "GPU buffers cannot bind CPU point ports");
    ExecutionPlan plan;
    for (std::uint64_t i = 1; i <= 5; ++i) {
        auto node = registry.MakeNode(i, "gpu.particles");
        node.properties_["particle_capacity"] = 262144.0;
        plan.instructions_.push_back({node, Operation::kGpuParticleEmitter, {{}, {}, {}}});
        Require(ValidatePointBudget(plan).has_value() == (i == 5), "aggregate GPU memory bounded");
    }
    plan.instructions_.resize(1);
    for (std::uint64_t i = 2; i <= 5; ++i) {
        plan.instructions_.push_back(
                {registry.MakeNode(i, "gpu.map"), Operation::kGpuPointMap, {std::size_t(i - 2)}});
        Require(ValidatePointBudget(plan).has_value() == (i == 5),
                "each derived buffer is counted in aggregate GPU budget");
    }
    doc.nodes_ = {registry.MakeNode(1, "gpu.particles"),  registry.MakeNode(2, "gpu.map"),
                  registry.MakeNode(3, "gpu.map"),        registry.MakeNode(4, "gpu.render"),
                  registry.MakeNode(5, "output.texture"), registry.MakeNode(6, "audio.band")};
    doc.edges_ = {{1, 1, 2, "points"},
                  {2, 2, 3, "points"},
                  {3, 3, 4, "points"},
                  {4, 4, 5, "source"},
                  {5, 6, 2, "point_size_scale"}};
    doc.output_ = 5;
    Require(std::holds_alternative<ExecutionPlan>(Compile(doc, registry)),
            "music-driven two-stage GPU mapping compiles");
    doc.nodes_[0] = registry.MakeNode(1, "point.grid");
    Require(std::holds_alternative<std::vector<Diagnostic>>(Compile(doc, registry)),
            "CPU point buffers cannot bind GPU mapping");
    doc.nodes_[0] = registry.MakeNode(1, "gpu.particles");
    doc.nodes_[1] = registry.MakeNode(2, "gpu.texture_sample");
    doc.nodes_.push_back(registry.MakeNode(7, "texture.gradient"));
    doc.edges_.back() = {5, 7, 2, "source"};
    const auto sampled = Compile(doc, registry);
    Require(std::holds_alternative<std::vector<Diagnostic>>(sampled),
            "mapping cannot silently drop a lazy texture view");
    const auto& errors = std::get<std::vector<Diagnostic>>(sampled);
    Require(errors.size() == 1 && errors.front().code_ == "graph.gpu_map_sample",
            "sample-before-map has an explicit actionable diagnostic");
    plan.instructions_.resize(1);
    plan.instructions_[0].node_.properties_["particle_capacity"] = 1.5;
    Require(ValidatePointBudget(plan).has_value(), "fractional GPU capacity rejected");
}
}  // namespace
int main() {
    try {
        Run();
        std::cout << "GPU particle graph contracts passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what();
        return 1;
    }
}
