#include <iostream>
#include <limits>
#include <stdexcept>

#include "rhythm/render/renderer.h"

namespace {
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
template <typename Function>
void Reject(Function function) {
    try {
        function();
    } catch (const std::exception&) {
        return;
    }
    throw std::runtime_error("invalid image program operation accepted");
}
void Run() {
    using namespace rhythm::render;
    // Null tests only resource admission. Actual target container/bytecode and
    // pixels are covered by image_shader_profile and both native GPU probes.
    std::vector<std::uint8_t> artifact(27, 0);
    artifact[0] = 'F';
    artifact[1] = 'S';
    artifact[2] = 'H';
    artifact[3] = 12;
    auto renderer = Renderer::CreateNull();
    auto foreign = Renderer::CreateNull();
    auto program = renderer.CreateImageProgram(artifact);
    const auto handle = program.Handle();
    Require(renderer.IsValid(handle) && !foreign.IsValid(handle),
            "image programs are device scoped");
    auto moved = std::move(program);
    Require(program.Handle() == ImageProgramHandle{} && moved.Handle() == handle,
            "image program ownership moves without duplicating the resource");
    auto target = renderer.CreateTexture({16, 16});
    const std::array<std::uint8_t, 4> white{255, 255, 255, 255};
    auto source = renderer.CreateTexture({1, 1}, white);
    DrawList list;
    list.width_ = list.height_ = 16;
    list.vertices_ = {{0, 0, 0, 0}, {16, 0, 1, 0}, {16, 16, 1, 1}};
    list.indices_ = {0, 1, 2};
    list.commands_ = {{source.Handle(), 0, 3, {0, 0, 16, 16}}};
    list.commands_[0].image_program_ = ImageProgramInput{handle};
    Reject([&] { renderer.Submit(target.Handle(), list); });
    renderer.BeginFrame();
    renderer.Submit(target.Handle(), list);
    auto other = foreign.CreateImageProgram(artifact);
    list.commands_[0].image_program_->program_ = other.Handle();
    Reject([&] { renderer.Submit(target.Handle(), list); });
    list.commands_[0].image_program_->program_ = handle;
    list.commands_[0].image_program_->parameters_[0] = std::numeric_limits<float>::quiet_NaN();
    Reject([&] { renderer.Submit(target.Handle(), list); });
    list.commands_[0].image_program_->parameters_[0] = 0;
    list.commands_[0].texture_noise_.emplace();
    Reject([&] { renderer.Submit(target.Handle(), list); });
    list.commands_[0].texture_noise_.reset();
    moved = {};
    auto replacement = renderer.CreateImageProgram(artifact);
    Require(replacement.Handle() != handle, "slot reuse advances the image program generation");
    Reject([&] { renderer.Submit(target.Handle(), list); });
    renderer.EndFrame();
    replacement = {};
    Require(renderer.Stats().image_programs_ == 0 && renderer.Stats().image_program_bytes_ == 0,
            "released image programs leave no admitted payload");
    std::vector<ImageProgram> programs;
    for (int i = 0; i < 64; ++i) programs.push_back(renderer.CreateImageProgram(artifact));
    Reject([&] { renderer.CreateImageProgram(artifact); });
    programs.clear();
    artifact.resize(512 * 1024);
    for (int i = 0; i < 32; ++i) programs.push_back(renderer.CreateImageProgram(artifact));
    Reject([&] { renderer.CreateImageProgram(artifact); });
    Require(renderer.Stats().image_program_bytes_ == 16 * 1024 * 1024,
            "compiled payload budget is independent of slot capacity");
    renderer.Invalidate();
    Require(!renderer.IsValid(programs[0].Handle()), "device loss invalidates program observers");
    programs.clear();
    std::cout << "Image program ownership, stale/foreign handles, draw validation, budgets and "
                 "loss passed\n";
}
}  // namespace
int main() {
    try {
        Run();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
