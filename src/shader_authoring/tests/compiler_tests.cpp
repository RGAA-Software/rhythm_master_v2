#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

#include "rhythm/prepared_assets/prepare.h"
#include "rhythm/project/store.h"
#include "rhythm/shader_authoring/compiler.h"

namespace {
void Require(bool value, std::string_view message) {
    if (!value) throw std::runtime_error(std::string(message));
}
rhythm::shader_authoring::Result Wait(rhythm::shader_authoring::Compiler& compiler) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(65);
    while (std::chrono::steady_clock::now() < deadline) {
        if (auto result = compiler.Take()) return *result;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    throw std::runtime_error("compiler worker timeout");
}
void Run(const rhythm::shader_authoring::Toolchain& tools, const std::filesystem::path& output,
         bool surface) {
    using namespace rhythm;
    shader_authoring::Compiler compiler;
    shader_authoring::Request request{
            tools, output / "project/assets",
            "mix(Sample(uv), vec4(uv.x, uv.y, sin(time) * 0.5 + 0.5, 1.0), clamp(a, 0.0, 1.0))"};
    if (surface) {
        request.profile_ = shader_expression::Profile::kSurfaceRgb;
        request.expression_ =
                "clamp(vec3(a, b, c) + abs(normal) * d + vec3(1.0, 1.0, 1.0) * "
                "sin(time + position.x + uv.y) * 0.1, 0.0, 1.0)";
    }
    Require(compiler.Start(request), "start compiler");
    Require(!compiler.Start(request), "one bounded compile job");
    auto result = Wait(compiler);
    if (!result.asset_) throw std::runtime_error(result.error_);
    const auto original = *result.asset_;
    const assets::Store store(request.assets_);
    const auto bytes = store.Read(original);
    const auto data = std::span<const std::uint8_t>(
            reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size());
    const auto expression = surface ? surface_shader::Decode(data).expression_
                                    : image_shader::Decode(data).expression_;
    Require(expression == request.expression_, "source travels with both target artifacts");
    graph::Registry registry;
    editor::Snapshot snapshot;
    snapshot.document_.id_ = "shader.authoring";
    snapshot.document_.nodes_ = {registry.MakeNode(1, "texture.shader"),
                                 registry.MakeNode(2, "output.texture")};
    snapshot.document_.nodes_[0].properties_["asset"] = original.id_;
    snapshot.document_.edges_ = {{1, 1, 2, "source"}};
    snapshot.document_.output_ = 2;
    if (surface) {
        snapshot.document_.nodes_ = {
                registry.MakeNode(1, "material.shader"), registry.MakeNode(2, "material.unlit"),
                registry.MakeNode(3, "geometry.sphere"), registry.MakeNode(4, "scene.instance"),
                registry.MakeNode(5, "scene.render"),    registry.MakeNode(6, "output.texture")};
        snapshot.document_.nodes_[0].properties_["asset"] = original.id_;
        snapshot.document_.edges_ = {{1, 2, 1, "material"},
                                     {2, 1, 4, "material"},
                                     {3, 3, 4, "geometry"},
                                     {4, 4, 5, "scene"},
                                     {5, 5, 6, "source"}};
        snapshot.document_.output_ = 6;
    }
    snapshot.assets_.push_back(original);
    project::Save(output / "project", snapshot);
    auto reopened = project::Load(output / "project");
    Require(reopened.snapshot_.assets_ == snapshot.assets_, "save/reopen shader reference");
    project::PublishSnapshot(output / "shader.rhythmpack", snapshot, request.assets_);
    const auto package = project::LoadPackage(output / "shader.rhythmpack");
    const auto prepared = prepared_assets::Prepare(package.program_, package.assets_);
    Require(prepared_assets::Covers(package.program_, *prepared) &&
                    (surface ? prepared->surfaces_->programs_.size()
                             : prepared->shaders_->programs_.size()) == 1,
            "published shader is prepared for Player");
    auto wrong = package.assets_;
    wrong[0].record_.media_type_ = "image/png";
    bool rejected = false;
    try {
        prepared_assets::Prepare(package.program_, wrong);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    Require(rejected, "wrong shader media type rejected");
    for (const auto invalid : {"while(true) {}", surface ? "vec2(uv)" : "vec4(uv, vec4(1.0))"}) {
        request.expression_ = invalid;
        Require(compiler.Start(request), "start invalid source");
        result = Wait(compiler);
        Require(!result.asset_ && !result.error_.empty(),
                "source/compiler diagnostic without asset publication");
        Require(store.Read(original) == bytes, "failed compile preserves immutable previous asset");
    }
    request.expression_ = surface ? "vec3(uv.x, uv.y, sin(time) * 0.5 + 0.5)"
                                  : "vec4(uv.x, uv.y, sin(time) * 0.5 + 0.5, 1.0)";
    Require(compiler.Start(request), "start replacement");
    result = Wait(compiler);
    if (!result.asset_) throw std::runtime_error(result.error_);
    Require(result.asset_->id_ != original.id_,
            "successful compile publishes distinct immutable asset");
    Require(store.Read(original) == bytes, "previous source remains available for undo");
    Require(compiler.Start(request), "start cancellable compile");
    compiler.Cancel();
    result = Wait(compiler);
    Require(!result.asset_ && result.error_ == "shader.cancelled",
            "cancel compile without publication");
    Require(std::filesystem::is_empty(output / "project/.shader-jobs"),
            "temporary compiler workspaces released");
}
}  // namespace
int main(int argc, char* argv[]) {
    try {
        if (argc != 5) throw std::invalid_argument("compiler, includes, varying, output required");
        const rhythm::shader_authoring::Toolchain tools{
                argv[1], argv[2], argv[3], std::filesystem::path(argv[3]).parent_path()};
        Run(tools, std::filesystem::path(argv[4]) / "image", false);
        Run(tools, std::filesystem::path(argv[4]) / "surface", true);
        std::cout << "Shader authoring: native dual compilation, diagnostics, cancellation, "
                     "replacement, save/package/prepare passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
