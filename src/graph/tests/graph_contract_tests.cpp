#include <iostream>
#include <limits>
#include <stdexcept>

#include "rhythm/graph/compiler.h"

namespace {
void Check(bool condition) {
    if (!condition) throw std::runtime_error("graph contract failed");
}
}  // namespace
int main() {
    using namespace rhythm::graph;
    try {
        Registry registry;
        Document document;
        document.id_ = "test.graph";
        document.nodes_ = {registry.MakeNode(1, "texture.gradient"),
                           registry.MakeNode(2, "output.texture"),
                           registry.MakeNode(3, "core.time")};
        document.output_ = 2;
        document.edges_ = {{1, 1, 2, "source"}};
        auto result = Compile(document, registry);
        Check(std::holds_alternative<ExecutionPlan>(result));
        Check(std::get<ExecutionPlan>(result).instructions_.size() == 2);
        const std::vector<NodeId> viewers{3};
        Check(std::get<ExecutionPlan>(Compile(document, registry, viewers)).instructions_.size() ==
              3);
        auto invalid = document;
        invalid.canvas_ = {1080, 1920};
        Check(std::get<ExecutionPlan>(Compile(invalid, registry)).canvas_ == invalid.canvas_);
        invalid.canvas_ = {4096, 4096};
        Check(std::holds_alternative<std::vector<Diagnostic>>(Compile(invalid, registry)));
        invalid.canvas_ = {0, 360};
        Check(std::holds_alternative<std::vector<Diagnostic>>(Compile(invalid, registry)));
        invalid = document;
        invalid.edges_[0].from_ = 3;
        Check(std::holds_alternative<std::vector<Diagnostic>>(Compile(invalid, registry)));
        invalid = document;
        invalid.nodes_.push_back(invalid.nodes_[0]);
        Check(std::holds_alternative<std::vector<Diagnostic>>(Compile(invalid, registry)));
        invalid = document;
        invalid.nodes_[0].properties_["color_a"] = std::numeric_limits<double>::quiet_NaN();
        Check(std::holds_alternative<std::vector<Diagnostic>>(Compile(invalid, registry)));
        document.nodes_.push_back(registry.MakeNode(4, "texture.transform"));
        document.edges_.push_back({2, 4, 4, "source"});
        Check(std::holds_alternative<std::vector<Diagnostic>>(Compile(document, registry)));
        document.nodes_.back() = registry.MakeNode(4, "texture.feedback");
        Check(std::holds_alternative<ExecutionPlan>(Compile(document, registry)));
        const std::vector<NodeId> feedback_viewer{4};
        const auto feedback = std::get<ExecutionPlan>(Compile(document, registry, feedback_viewer));
        Check(feedback.instructions_.size() == 3);
        Check(feedback.instructions_.back().inputs_[0] == 2);
        std::cout << "graph contracts passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
