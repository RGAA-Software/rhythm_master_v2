#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include "rhythm/runtime/runtime.h"
#include "shader_pass.h"

namespace {
void Require(bool condition, std::string_view message) {
    if (!condition) throw std::runtime_error(std::string(message));
}
std::vector<std::uint8_t> Read(const std::filesystem::path& path) {
    const auto size = std::filesystem::file_size(path);
    if (size > rhythm::image_shader::kMaximumArtifactBytes) throw std::length_error("fixture");
    std::vector<std::uint8_t> bytes(std::size_t(size), 0);
    std::ifstream input(path, std::ios::binary);
    input.read(reinterpret_cast<char*>(bytes.data()), std::streamsize(bytes.size()));
    Require(bool(input), "fixture read");
    return bytes;
}
void Run(const std::filesystem::path& windows, const std::filesystem::path& android) {
    using namespace rhythm;
    image_shader::Program program;
    program.expression_ =
            "mix(Sample(uv), vec4(uv.x, uv.y, sin(time) * 0.5 + 0.5, 1.0), clamp(a, 0.0, 1.0))";
    program.compiler_sha256_ = std::string(64, 'a');
    program.artifacts_ = {Read(windows), Read(android)};
    image_shader::Validate(program);
    const assets::AssetId id{std::string(64, 'a')}, next_id{std::string(64, 'b')};
    auto resources = std::make_shared<image_shader::Resources>();
    resources->programs_.emplace(id.sha256_, program);
    resources->programs_.emplace(next_id.sha256_, program);
    graph::Registry registry;
    graph::Document document;
    document.id_ = "shader.runtime";
    document.nodes_ = {
            registry.MakeNode(1, "texture.shader"), registry.MakeNode(2, "texture.shader"),
            registry.MakeNode(3, "output.texture"), registry.MakeNode(4, "audio.feature")};
    for (int index = 0; index < 2; ++index) {
        document.nodes_[index].properties_["asset"] = id;
        document.nodes_[index].properties_["texture_precision"] = 2.0;
    }
    document.edges_ = {{1, 1, 2, "source"}, {2, 2, 3, "source"}, {3, 4, 2, "a"}};
    document.output_ = 3;
    auto plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
    auto renderer = render::Renderer::CreateNull();
    runtime::Runtime runtime;
    runtime::FrameContext frame;
    frame.shaders_ = resources;
    frame.extent_ = {64, 32};
    frame.external_.audio_.emplace();
    frame.external_.audio_->generation_ = 1;
    renderer.BeginFrame();
    const auto first = runtime.Evaluate(plan, frame, renderer);
    Require(renderer.Precision(first.final_) == render::TexturePrecision::kFloat16,
            "float shader target");
    renderer.EndFrame();
    Require(renderer.Stats().image_programs_ == 1, "shared shader program");
    const auto bytes = renderer.Stats().image_program_bytes_;
    for (int index = 0; index < 10; ++index) {
        frame.seconds_ = index * 0.1;
        renderer.BeginFrame();
        runtime.Evaluate(plan, frame, renderer);
        renderer.EndFrame();
        Require(renderer.Stats().image_programs_ == 1 &&
                        renderer.Stats().image_program_bytes_ == bytes,
                "time and parameters retain program");
    }
    frame.advance_state_ = false;
    renderer.BeginFrame();
    runtime.Evaluate(plan, frame, renderer);
    renderer.EndFrame();
    renderer.BeginFrame();
    Require(runtime.Evaluate(plan, frame, renderer).evaluated_ == 0, "paused shader frame cached");
    renderer.EndFrame();
    // Inspect uniforms independently of Null's intentionally non-rasterizing backend.
    runtime::detail::ShaderPrograms cache;
    cache.Retain(plan, *resources);
    const auto found = std::find_if(plan.instructions_.begin(), plan.instructions_.end(),
                                    [](const auto& value) { return value.node_.id_ == 2; });
    auto outputs = first.outputs_;
    outputs[*found->inputs_[2]].scalar_ = 0.75;
    auto draw = cache.Draw(*found, outputs, 100000, {}, {64, 32}, *resources, renderer);
    const auto original = draw.commands_[0].image_program_->program_;
    Require(draw.commands_[0].image_program_->parameters_[0] == 0.75f &&
                    draw.commands_[0].image_program_->seconds_ == 100000,
            "audio and unified time uniforms");
    outputs[*found->inputs_[2]].scalar_ = 0.25;
    draw = cache.Draw(*found, outputs, 2, {}, {64, 32}, *resources, renderer);
    Require(draw.commands_[0].image_program_->program_ == original &&
                    draw.commands_[0].image_program_->parameters_[0] == 0.25f,
            "uniform edit reuses handle");
    cache = {};
    for (int index = 0; index < 2; ++index) document.nodes_[index].properties_["asset"] = next_id;
    ++document.revision_;
    plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
    renderer.BeginFrame();
    Require(runtime.Evaluate(plan, frame, renderer).evaluated_ >= 2,
            "paused asset replacement redraws");
    renderer.EndFrame();
    Require(renderer.Stats().image_programs_ == 1, "old shader released after replacement");
    frame.shaders_.reset();
    bool missing = false;
    renderer.BeginFrame();
    try {
        runtime.Evaluate(plan, frame, renderer);
    } catch (const std::invalid_argument&) {
        missing = true;
    }
    renderer.EndFrame();
    Require(missing, "missing shader cannot reuse stale frame");
    runtime.Reset();
    Require(renderer.Stats().image_programs_ == 0 && renderer.Stats().texture_bytes_ == 0,
            "runtime releases shader assets");
}
}  // namespace
int main(int argc, char* argv[]) {
    try {
        if (argc != 3) throw std::invalid_argument("requires Windows and GLES compiler fixtures");
        Run(argv[1], argv[2]);
        std::cout << "Shader runtime: shared uploads, music/time, float targets, pause, "
                     "replacement and release passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
