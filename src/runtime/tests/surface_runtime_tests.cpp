#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include "rhythm/runtime/runtime.h"
#include "rhythm/runtime/viewers.h"

namespace {
void Require(bool value) {
    if (!value) throw std::runtime_error("surface.runtime_contract");
}
std::vector<std::uint8_t> Read(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    const auto size = file.tellg();
    if (!file || size < 24 || size > 512 * 1024) throw std::runtime_error("surface.fixture");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(bytes.data()), size);
    if (!file) throw std::runtime_error("surface.read");
    return bytes;
}
}  // namespace
int main(int argc, char* argv[]) {
    using namespace rhythm;
    try {
        if (argc != 2) throw std::runtime_error("surface.fixture_directory_required");
        const std::filesystem::path directory(argv[1]);
        surface_shader::Program program;
        program.expression_ = "clamp(vec3(a, b, c) * d, vec3(0.0, 0.0, 0.0), vec3(1.0, 1.0, 1.0))";
        program.compiler_sha256_ = std::string(64, 'a');
        program.artifacts_ = {Read(directory / "windows/parameter_surface.bin"),
                              Read(directory / "android/parameter_surface.bin")};
        surface_shader::Validate(program);
        const assets::AssetId id{std::string(64, 'a')}, next{std::string(64, 'b')};
        auto resources = std::make_shared<surface_shader::Resources>();
        resources->programs_.emplace(id.sha256_, program);
        resources->programs_.emplace(next.sha256_, program);
        graph::Registry registry;
        graph::Document document;
        document.id_ = "surface.runtime";
        document.nodes_ = {
                registry.MakeNode(1, "geometry.sphere"), registry.MakeNode(2, "material.unlit"),
                registry.MakeNode(3, "material.shader"), registry.MakeNode(4, "material.shader"),
                registry.MakeNode(5, "scene.instance"),  registry.MakeNode(6, "scene.instance"),
                registry.MakeNode(7, "scene.merge"),     registry.MakeNode(8, "scene.render"),
                registry.MakeNode(9, "output.texture"),  registry.MakeNode(10, "scalar.constant")};
        document.nodes_[2].properties_["asset"] = id;
        document.nodes_[3].properties_["asset"] = id;
        document.nodes_[9].properties_["value"] = .75;
        document.edges_ = {{1, 2, 3, "material"}, {2, 2, 4, "material"}, {3, 1, 5, "geometry"},
                           {4, 3, 5, "material"}, {5, 1, 6, "geometry"}, {6, 4, 6, "material"},
                           {7, 5, 7, "a"},        {8, 6, 7, "b"},        {9, 7, 8, "scene"},
                           {10, 8, 9, "source"},  {11, 10, 3, "a"}};
        document.output_ = 9;
        auto plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
        auto renderer = render::Renderer::CreateNull();
        runtime::Runtime runtime;
        runtime::FrameContext frame;
        frame.surfaces_ = resources;
        frame.extent_ = {64, 32};
        frame.seconds_ = 100000;
        const auto evaluate = [&] {
            renderer.BeginFrame();
            auto result = runtime.Evaluate(plan, frame, renderer);
            renderer.EndFrame();
            return result;
        };
        const auto first = evaluate();
        Require(renderer.IsValid(first.final_) && renderer.Stats().surface_programs_ == 1);
        const auto output = std::find_if(first.outputs_.begin(), first.outputs_.end(),
                                         [](const auto& item) { return item.node_ == 3; });
        Require(output != first.outputs_.end() && output->material_->surface_node_ == 3 &&
                output->surface_program_->parameters_[0] == .75f &&
                output->surface_program_->seconds_ == 100000);
        const auto handle = output->surface_program_->program_;
        {
            runtime::Viewers viewers;
            Require(viewers.BeginFrame(frame.seconds_, true, 0));
            const std::array<graph::NodeId, 3> nodes{3, 4, 7};
            renderer.BeginFrame();
            viewers.Capture(first, nodes, renderer);
            renderer.EndFrame();
            Require(viewers.Outputs().size() == 3 && renderer.Stats().surface_programs_ == 1);
            for (const auto& preview : viewers.Outputs())
                Require(renderer.IsValid(preview.texture_));
        }
        Require(evaluate().evaluated_ == 0);
        frame.seconds_ += 1;
        Require(evaluate().evaluated_ > 0 && renderer.IsValid(handle));
        document.edges_.push_back({12, 10, 3, "time"});
        document.edges_.push_back({13, 10, 4, "time"});
        ++document.revision_;
        plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
        evaluate();
        frame.seconds_ += 1;
        Require(evaluate().evaluated_ == 0);
        for (int index : {2, 3}) document.nodes_[index].properties_["asset"] = next;
        ++document.revision_;
        plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
        const auto changed = evaluate();
        Require(!renderer.IsValid(handle) && renderer.Stats().surface_programs_ == 1 &&
                renderer.IsValid(changed.final_));
        runtime.Reset();
        Require(renderer.Stats().surface_programs_ == 0);
        runtime.BeginPreparation(plan, frame);
        runtime::PreparationProgress progress;
        for (std::size_t step = 0; step <= plan.instructions_.size(); ++step) {
            renderer.BeginFrame();
            progress = runtime.PrepareNext(renderer, {1, 100});
            renderer.EndFrame();
            if (progress.state_ != runtime::PreparationState::kPending) break;
        }
        Require(progress.state_ == runtime::PreparationState::kReady && progress.output_ &&
                renderer.IsValid(progress.output_->final_) &&
                renderer.Stats().surface_programs_ == 1);
        runtime.Reset();
        Require(renderer.Stats().surface_programs_ == 0);
        std::cout << "Surface runtime: shared asset, producer identity, scalar/time inputs, pause, "
                     "explicit clock, replacement and reset passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
