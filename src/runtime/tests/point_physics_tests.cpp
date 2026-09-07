#include <cmath>
#include <iostream>
#include <stdexcept>

#include "point_physics.h"

namespace {
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void Run() {
    using namespace rhythm;
    graph::Registry registry;
    graph::Document document;
    document.id_ = "physics-test";
    document.nodes_ = {registry.MakeNode(1, "point.grid"), registry.MakeNode(2, "point.physics2d"),
                       registry.MakeNode(3, "point.render"),
                       registry.MakeNode(4, "output.texture")};
    document.nodes_[0].properties_["columns"] = 1.0;
    document.nodes_[0].properties_["rows"] = 1.0;
    document.nodes_[0].properties_["center_y"] = 0.2;
    document.nodes_[0].properties_["point_size"] = 0.1;
    document.nodes_[1].properties_["restitution"] = 0.0;
    document.edges_ = {{1, 1, 2, "points"}, {2, 2, 3, "points"}, {3, 3, 4, "source"}};
    document.output_ = 4;
    const auto compiled = graph::Compile(document, registry);
    Require(std::holds_alternative<graph::ExecutionPlan>(compiled), "physics point graph compiles");
    const auto& plan = std::get<graph::ExecutionPlan>(compiled);
    auto renderer = render::Renderer::CreateNull();
    runtime::Runtime runtime;
    runtime::FrameContext context;
    context.extent_ = {640, 360};
    const auto evaluate = [&] {
        renderer.BeginFrame();
        auto result = runtime.Evaluate(plan, context, renderer);
        renderer.EndFrame();
        return result;
    };
    auto first = evaluate();
    const auto initial = first.outputs_[1].points_;
    for (int frame = 1; frame <= 180; ++frame) {
        context.seconds_ = frame / 60.0;
        first = evaluate();
    }
    const auto landed = first.outputs_[1].points_->front();
    Require(landed.y_ > 0.94f && landed.y_ < 0.96f && std::abs(landed.x_ - 0.5f) < 0.001f,
            "circle floor contact uses height-relative radius on a wide canvas");
    Require(initial->front().y_ == 0.2f, "previous physics snapshots remain immutable");
    context.advance_state_ = false;
    context.seconds_ = 4;
    const auto paused = evaluate();
    Require(*paused.outputs_[1].points_ == *first.outputs_[1].points_,
            "pause preserves body state");
    context.reset_generation_++;
    const auto restarted = evaluate();
    Require(restarted.outputs_[1].points_->front().y_ == 0.2f, "restart resets bodies");
    document.nodes_[0].properties_["columns"] = 32.0;
    document.nodes_[0].properties_["rows"] = 32.0;
    Require(!std::holds_alternative<graph::ExecutionPlan>(graph::Compile(document, registry)),
            "physics budget rejects before native allocation");

    runtime::detail::PointPhysics physics;
    graph::Instruction instruction{registry.MakeNode(2, "point.physics2d"),
                                   graph::Operation::kPointPhysics,
                                   {0, std::nullopt, std::nullopt}};
    instruction.node_.properties_["physics_bounds"] = 0.0;
    instruction.node_.properties_["gravity_y"] = 0.0;
    particles::Point point;
    point.id_ = 42;
    point.x_ = point.y_ = 0.5f;
    point.size_ = 0.02f;
    point.velocity_x_ = 0.1f;
    std::vector<runtime::NodeOutput> inputs(1);
    inputs[0].points_ = std::make_shared<const particles::PointCloud>(particles::PointCloud{point});
    context = {};
    context.extent_ = {640, 360};
    physics.Evaluate(instruction, inputs, context);
    context.seconds_ = 0.1;
    const auto moving = physics.Evaluate(instruction, inputs, context);
    Require(std::abs(moving->front().x_ - 0.51f) < 1e-5,
            "birth velocity survives normalized width to meters conversion");
    ++inputs[0].points_generation_;
    Require(physics.Evaluate(instruction, inputs, context)->front().x_ == 0.5f,
            "source generation reset retires old bodies even when point IDs repeat");
    inputs[0].points_ = std::make_shared<const particles::PointCloud>();
    Require(physics.Evaluate(instruction, inputs, context)->empty(),
            "source retirement removes body");
    inputs[0].points_ = std::make_shared<const particles::PointCloud>(particles::PointCloud{point});
    const auto reborn = physics.Evaluate(instruction, inputs, context);
    Require(reborn->front().x_ == 0.5f, "retired identity can restart as a fresh source body");
    std::cout << "point physics: typed graph, floor contact, pause/reset, velocity, retirement and "
                 "budget passed\n";
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
