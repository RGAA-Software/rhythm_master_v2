#include <bgfx/bgfx.h>

#include <filesystem>
#include <iostream>
#include <stdexcept>

#include "rhythm/assets/store.h"
#include "rhythm/platform/host.h"
#include "rhythm/player/session.h"
#include "scene_fixture.h"
#include "scene_graph_fixture.h"

int main(int argc, char* argv[]) {
    try {
        if (argc != 3) throw std::invalid_argument("probe.arguments");
        rhythm::platform::Host host(true);
        auto renderer = host.CreateRenderer();
        rhythm::validation::SceneFixture fixture(renderer);
        for (int scenario = 0; scenario < 13; ++scenario) {
            const auto path =
                    (std::filesystem::path(argv[1]) / ("scene-" + std::to_string(scenario)))
                            .string();
            for (int frame = 0; frame < 8; ++frame) {
                fixture.Draw(renderer, scenario);
                // Native screenshot boundary only; the standard bgfx callback
                // writes its copied frame to the test's unique output directory.
                if (frame == 3) bgfx::requestScreenShot(BGFX_INVALID_HANDLE, path.c_str());
            }
        }
        rhythm::validation::SceneGraphFixture graph_fixture;
        for (int scenario = 0; scenario < 9; ++scenario) {
            const auto path =
                    (std::filesystem::path(argv[1]) / ("graph-" + std::to_string(scenario)))
                            .string();
            for (int frame = 0; frame < 8; ++frame) {
                graph_fixture.Draw(renderer, scenario);
                if (frame == 3) bgfx::requestScreenShot(BGFX_INVALID_HANDLE, path.c_str());
            }
        }
        rhythm::assets::Store store(std::filesystem::path(argv[1]) / "assets");
        const auto asset = store.Import(argv[2], "model/gltf-binary");
        const std::vector<rhythm::project::PackagedAsset> assets{{asset, store.Read(asset)}};
        rhythm::graph::Registry registry;
        rhythm::graph::Document document;
        document.id_ = "model.gpu";
        document.canvas_ = {16, 16};
        document.nodes_ = {
                registry.MakeNode(1, "geometry.glb"),   registry.MakeNode(2, "material.unlit"),
                registry.MakeNode(3, "scene.instance"), registry.MakeNode(4, "scene.transform"),
                registry.MakeNode(5, "scene.render"),   registry.MakeNode(6, "output.texture")};
        document.nodes_[0].properties_["asset"] = asset.id_;
        document.nodes_[1].properties_["color_a"] = rhythm::graph::Color{0, 1, 0, 1};
        document.edges_ = {{1, 1, 3, "geometry"},
                           {2, 2, 3, "material"},
                           {3, 3, 4, "scene"},
                           {4, 4, 5, "scene"},
                           {5, 5, 6, "source"}};
        document.output_ = 6;
        rhythm::player::Session session;
        for (int scenario = 0; scenario < 3; ++scenario) {
            if (scenario != 1) {
                document.nodes_[3].properties_["translate_x"] = scenario == 2 ? 3.0 : 0.0;
                session.Load(rhythm::project::EncodePackage(document, "Cesium Box test", assets));
            } else
                session.ReleaseGraphics();
            const auto path =
                    (std::filesystem::path(argv[1]) / ("model-" + std::to_string(scenario)))
                            .string();
            for (int frame = 0; frame < 8; ++frame) {
                renderer.BeginFrame();
                const auto output = session.Tick(0, false, {16, 16}, renderer);
                rhythm::render::DrawList draw;
                draw.width_ = draw.height_ = 16;
                draw.vertices_ = {{0, 0, 0, 0}, {16, 0, 1, 0}, {16, 16, 1, 1}, {0, 16, 0, 1}};
                draw.indices_ = {0, 1, 2, 0, 2, 3};
                draw.commands_ = {{output.final_, 0, 6, {0, 0, 16, 16}}};
                renderer.Submit({}, draw);
                if (frame == 3) bgfx::requestScreenShot(BGFX_INVALID_HANDLE, path.c_str());
                renderer.EndFrame();
            }
        }
        std::cout << "D3D scene captures: depth order, front/back culling, double side and depth "
                     "clear\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
