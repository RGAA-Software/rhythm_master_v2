#include <iostream>
#include <limits>
#include <stdexcept>

#include "rhythm/render/renderer.h"

namespace {
template <typename Function>
void Reject(Function function) {
    try {
        function();
    } catch (const std::exception&) {
        return;
    }
    throw std::runtime_error("surface.invalid_operation_accepted");
}
void Require(bool value) {
    if (!value) throw std::runtime_error("surface.resource_contract");
}
}  // namespace
int main() {
    using namespace rhythm::render;
    try {
        // Null checks lifecycle/admission only; actual artifacts and GPU output
        // have separate two-platform fixtures.
        std::vector<std::uint8_t> artifact(27, 0);
        artifact[0] = 'F';
        artifact[1] = 'S';
        artifact[2] = 'H';
        artifact[3] = 12;
        auto renderer = Renderer::CreateNull();
        auto foreign = Renderer::CreateNull();
        auto program = renderer.CreateSurfaceProgram(artifact);
        const auto handle = program.Handle();
        Require(renderer.IsValid(handle) && !foreign.IsValid(handle));
        auto moved = std::move(program);
        Require(program.Handle() == SurfaceProgramHandle{} && moved.Handle() == handle);
        constexpr std::array<MeshVertex, 3> kVertices{{{-1, -1, 0}, {1, -1, 0}, {0, 1, 0}}};
        constexpr std::array<std::uint32_t, 3> kIndices{0, 1, 2};
        auto mesh = renderer.CreateMesh(kVertices, kIndices);
        auto color = renderer.CreateTexture({16, 16});
        auto depth = renderer.CreateDepthTexture({16, 16});
        SceneDrawList scene;
        MeshDraw draw;
        draw.mesh_ = mesh.Handle();
        draw.surface_program_ = SurfaceProgramInput{handle};
        scene.draws_ = {draw};
        Reject([&] { renderer.SubmitScene(color.Handle(), scene); });
        renderer.BeginFrame();
        renderer.SubmitScene(color.Handle(), scene);
        renderer.SubmitSceneDepth(color.Handle(), depth.Handle(), scene);
        auto other = foreign.CreateSurfaceProgram(artifact);
        scene.draws_[0].surface_program_->program_ = other.Handle();
        Reject([&] { renderer.SubmitScene(color.Handle(), scene); });
        Reject([&] { renderer.SubmitSceneDepth(color.Handle(), depth.Handle(), scene); });
        scene.draws_[0].surface_program_ = SurfaceProgramInput{handle};
        scene.draws_[0].surface_program_->parameters_[0] = std::numeric_limits<float>::quiet_NaN();
        Reject([&] { renderer.SubmitScene(color.Handle(), scene); });
        scene.draws_[0].surface_program_->parameters_[0] = 0;
        scene.draws_[0].surface_program_->seconds_ = -1;
        Reject([&] { renderer.SubmitScene(color.Handle(), scene); });
        scene.draws_[0].surface_program_->seconds_ = 0;
        moved = {};
        auto replacement = renderer.CreateSurfaceProgram(artifact);
        Require(replacement.Handle() != handle && !renderer.IsValid(handle));
        Reject([&] { renderer.SubmitScene(color.Handle(), scene); });
        renderer.EndFrame();
        replacement = {};
        Require(renderer.Stats().surface_programs_ == 0 &&
                renderer.Stats().surface_program_bytes_ == 0);
        std::vector<SurfaceProgram> programs;
        for (int index = 0; index < 32; ++index)
            programs.push_back(renderer.CreateSurfaceProgram(artifact));
        Reject([&] { renderer.CreateSurfaceProgram(artifact); });
        renderer.Invalidate();
        Require(!renderer.IsValid(programs[0].Handle()));
        programs.clear();
        std::cout << "Surface resource contracts: ownership, device/generation isolation, "
                     "color/depth draw admission, limits and loss passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
