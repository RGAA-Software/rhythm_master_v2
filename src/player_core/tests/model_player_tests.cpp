#include <fstream>
#include <iostream>
#include <stdexcept>

#include "rhythm/assets/store.h"
#include "rhythm/player/session.h"

namespace {
void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}  // namespace
int main(int argc, char* argv[]) {
    using namespace rhythm;
    try {
        Check(argc == 3, "fixture and scratch paths required");
        assets::Store store(argv[2]);
        const auto record = store.Import(argv[1], "model/gltf-binary");
        const std::vector<project::PackagedAsset> assets{{record, store.Read(record)}};
        graph::Registry registry;
        graph::Document document;
        document.id_ = "model.player";
        document.nodes_ = {
                registry.MakeNode(1, "geometry.glb"), registry.MakeNode(2, "scene.instance"),
                registry.MakeNode(3, "scene.render"), registry.MakeNode(4, "output.texture")};
        document.nodes_[0].properties_["asset"] = record.id_;
        document.edges_ = {{1, 1, 2, "geometry"}, {2, 2, 3, "scene"}, {3, 3, 4, "source"}};
        document.output_ = 4;
        const auto bytes = project::EncodePackage(document, "Cesium Box", assets);
        player::PreparedPackage prepared(bytes);
        player::PreparedPackage moved(std::move(prepared));
        Check(!prepared.Ready() && moved.Ready(), "resource ownership moves with package");
        player::Session session;
        session.LoadPrepared(std::move(moved));
        auto renderer = render::Renderer::CreateNull();
        auto tick = [&] {
            renderer.BeginFrame();
            const auto result = session.Tick(0, false, {64, 64}, renderer);
            renderer.EndFrame();
            return result;
        };
        const auto first = tick();
        Check(renderer.IsValid(first.final_) && renderer.Stats().live_meshes_ == 1,
              "embedded GLB reaches player GPU resources");
        auto corrupt = document;
        const auto invalid = store.Import(
                std::filesystem::path(argv[1]).parent_path() / "README.md", "text/plain");
        corrupt.nodes_[0].properties_["asset"] = invalid.id_;
        bool rejected = false;
        try {
            session.Load(project::EncodePackage(
                    corrupt, "invalid",
                    std::vector<project::PackagedAsset>{{invalid, store.Read(invalid)}}));
        } catch (const std::exception&) {
            rejected = true;
        }
        Check(rejected && session.Title() == "Cesium Box" && renderer.IsValid(first.final_),
              "invalid model preserves the active package and frame");
        session.ReleaseGraphics();
        Check(renderer.Stats().live_meshes_ == 0, "surface release frees geometry");
        Check(renderer.IsValid(tick().final_) && renderer.Stats().live_meshes_ == 1,
              "surface recreation restores prepared geometry");
        std::cout << "Model Player lifecycle passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
