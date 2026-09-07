#include <iostream>
#include <stdexcept>

#include "rhythm/graph/bindings.h"
#include "rhythm/graph/compiler.h"

namespace {
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
void Run() {
    using namespace rhythm::graph;
    Registry registry;
    Document document;
    document.id_ = "bindings";
    document.nodes_ = {registry.MakeNode(1, "scalar.constant"),
                       registry.MakeNode(2, "texture.gradient"),
                       registry.MakeNode(3, "output.texture")};
    document.output_ = 3;
    document.edges_ = {{1, 2, 3, "source"}};
    document.signals_ = {{"brightness", 1}};
    document.bindings_ = {{2, "amount", "brightness"}};
    const auto result = Compile(document, registry);
    Require(std::holds_alternative<ExecutionPlan>(result), "named input compiles");
    const auto plan = std::get<ExecutionPlan>(result);
    Require(plan.instructions_.size() == 3 && plan.instructions_[1].inputs_[0] == 0,
            "named dependency order and demand");
    auto wired = document;
    wired.bindings_.clear();
    wired.signals_.clear();
    wired.edges_.push_back({2, 1, 2, "amount"});
    const auto wired_plan = std::get<ExecutionPlan>(Compile(wired, registry));
    Require(wired_plan.instructions_[1].inputs_ == plan.instructions_[1].inputs_,
            "binding/wire compilation parity");
    auto invalid = document;
    invalid.signals_.push_back({"brightness", 2});
    Require(std::holds_alternative<std::vector<Diagnostic>>(Compile(invalid, registry)),
            "duplicate signal rejects");
    invalid = document;
    invalid.bindings_[0].signal_ = "missing";
    Require(std::holds_alternative<std::vector<Diagnostic>>(Compile(invalid, registry)),
            "missing signal rejects");
    invalid = document;
    invalid.edges_.push_back({2, 1, 2, "amount"});
    Require(std::holds_alternative<std::vector<Diagnostic>>(Compile(invalid, registry)),
            "wire/binding input conflict");
    invalid = document;
    invalid.signals_[0].source_ = 2;
    Require(std::holds_alternative<std::vector<Diagnostic>>(Compile(invalid, registry)),
            "incompatible binding type");
    Require(!ValidSignalName("bad\nname") && !ValidSignalName("bad##id") &&
                    !ValidSignalName(std::string(129, 'a')),
            "binding name bounds");
    std::cout << "named graph binding contracts passed\n";
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
