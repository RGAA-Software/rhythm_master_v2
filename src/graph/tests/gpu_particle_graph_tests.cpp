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
