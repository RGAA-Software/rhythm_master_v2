#include <bgfx/bgfx.h>

#include <filesystem>
#include <iostream>
#include <stdexcept>

#include "rhythm/assets/store.h"
#include "rhythm/platform/host.h"
#include "rhythm/player/session.h"

int main(int argc, char** argv) {
    using namespace rhythm;
    try {
        if (argc != 3) throw std::invalid_argument("image GPU fixture and output required");
        const std::filesystem::path directory(argv[2]);
        assets::Store store(directory / "assets");
        const auto record = store.Import(argv[1], "image/png");
        const std::vector<project::PackagedAsset> assets{{record, store.Read(record)}};
        graph::Registry registry;
        graph::Document document;
        document.id_ = "image.gpu";
        document.canvas_ = {128, 128};
        document.nodes_ = {registry.MakeNode(1, "texture.image"),
                           registry.MakeNode(2, "output.texture")};
        document.nodes_[0].properties_["asset"] = record.id_;
        document.edges_ = {{1, 1, 2, "source"}};
        document.output_ = 2;
        platform::Host host(true);
        auto renderer = host.CreateRenderer();
        player::Session session;
        const std::array<std::uint8_t, 4> original_pixel{255, 0, 0, 255};
        auto dynamic_image = renderer.CreateTexture({1, 1}, original_pixel);
        for (int scenario = 0; scenario < 5; ++scenario) {
            if (scenario < 2) {
                document.nodes_[0].properties_["image_fill"] = double(scenario);
                session.Load(project::EncodePackage(document, "Image GPU", assets));
            } else if (scenario == 2) {
                session.ReleaseGraphics();
            }
            const render::Extent extent{128, static_cast<std::uint16_t>(scenario == 3 ? 64 : 128)};
            const auto capture = (directory / ("image-" + std::to_string(scenario))).string();
            for (int frame = 0; frame < 8; ++frame) {
                renderer.BeginFrame();
                render::TextureHandle image;
                if (scenario == 4) {
                    const std::array<std::uint8_t, 4> updated_pixel{120, 200, 40, 128};
                    renderer.UpdateTexture(dynamic_image.Handle(), updated_pixel);
                    image = dynamic_image.Handle();
                } else {
                    image = session.Tick(0, false, extent, renderer).final_;
                }
                render::DrawList draw;
                draw.width_ = extent.width_;
                draw.height_ = extent.height_;
                const float width = extent.width_, height = extent.height_;
                draw.vertices_ = {
                        {0, 0, 0, 0}, {width, 0, 1, 0}, {width, height, 1, 1}, {0, height, 0, 1}};
                draw.indices_ = {0, 1, 2, 0, 2, 3};
                draw.commands_ = {{image, 0, 6, {0, 0, width, height}}};
                renderer.Submit({}, draw, 0x000000ff);
                if (frame == 3) bgfx::requestScreenShot(BGFX_INVALID_HANDLE, capture.c_str());
                renderer.EndFrame();
            }
        }
        std::cout << "Image D3D11 captures completed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
