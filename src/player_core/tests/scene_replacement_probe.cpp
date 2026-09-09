#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

#include "rhythm/player/scene_deck.h"
#include "scene_compositor_probe.h"

namespace rhythm::validation {
namespace {
void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
std::string Package(const std::string& title, graph::Color color) {
    graph::Registry registry;
    graph::Document document;
    document.id_ = title;
    document.canvas_ = {32, 32};
    document.nodes_ = {registry.MakeNode(1, "texture.gradient")};
    for (const auto key : {"color_a", "color_b"}) document.nodes_[0].properties_[key] = color;
    for (std::uint64_t id = 2; id <= 60; ++id) {
        document.nodes_.push_back(registry.MakeNode(id, "texture.transform"));
        document.nodes_.back().properties_["scale"] = 1.0;
        document.edges_.push_back({id, id - 1, id, "source"});
    }
    document.nodes_.push_back(registry.MakeNode(61, "output.texture"));
    document.edges_.push_back({61, 60, 61, "source"});
    document.output_ = 61;
    return project::EncodePackage(document, title);
}
void Pixels(render::Renderer& renderer, render::Readback& ticket, bool blue) {
    for (unsigned frame = 0; frame < 16; ++frame) {
        if (const auto image = ticket.Poll()) {
            Check(image->rgba_.size() == 32 * 32 * 4, "serial scene readback extent");
            for (std::size_t offset = 0; offset < image->rgba_.size(); offset += 4)
                Check(image->rgba_[offset + (blue ? 2 : 0)] >= 253 &&
                              image->rgba_[offset + (blue ? 0 : 2)] <= 2 &&
                              image->rgba_[offset + 1] <= 2 && image->rgba_[offset + 3] >= 253,
                      "serial scene accepted/frozen/recovered pixels");
            return;
        }
        renderer.BeginFrame();
        renderer.EndFrame();
    }
    throw std::runtime_error("serial scene readback deadline");
}
}  // namespace
void VerifySceneReplacement(render::Renderer& renderer, const std::filesystem::path& root) {
    const auto baseline = renderer.Stats().texture_bytes_;
    std::filesystem::create_directories(root);
    const auto path = root / "serial-blue.rhythmpack";
    {
        std::ofstream file(path, std::ios::binary);
        file << Package("Blue candidate", {0, 0, 1, 1});
        Check(bool(file), "write serial GPU fixture");
    }
    {
        auto pressure = renderer.CreateTexture({8192, 8182});
        player::SceneDeck deck;
        deck.LoadPrepared(player::PreparedPackage(Package("Red accepted", {1, 0, 0, 1})));
        player::SceneQueue queue;
        const auto id = queue.Enqueue(path, "Blue candidate").value();
        double seconds = 0;
        const auto tick = [&](bool inspect = false, bool blue = false) {
            queue.Pump(deck.CanPrepareNext());
            renderer.BeginFrame();
            auto frame = deck.Tick(seconds += 0.01, false, player::RenderQuality::kOriginal,
                                   renderer, {}, {}, queue);
            Check(renderer.IsValid(frame.output_.final_), "serial GPU preserves accepted output");
            auto ticket =
                    inspect ? renderer.RequestReadback(frame.output_.final_) : render::Readback{};
            renderer.EndFrame();
            if (inspect) Pixels(renderer, ticket, blue);
            return frame;
        };
        const auto wait = [&] {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
            while (std::chrono::steady_clock::now() < deadline) {
                tick();
                if (deck.CanHardCut(id)) return;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            throw std::runtime_error("serial GPU admission deadline");
        };
        wait();
        tick(true);
        Check(deck.RequestHardCut(id), "explicit GPU hard cut");
        Check(tick(true).output_.outputs_.empty(), "freeze exposes only retained final output");
        tick(true);
        deck.CancelTransition();
        for (unsigned step = 0; step < 30 && deck.RestoringGraphics(); ++step) tick(true);
        Check(!deck.RestoringGraphics() && deck.Current().Title() == "Red accepted",
              "GPU cancellation recovers old graph");
        Check(queue.Retry(), "serial GPU retry row");
        wait();
        Check(deck.RequestHardCut(id), "serial GPU retry action");
        tick(true);
        bool switched = false;
        for (unsigned step = 0; step < 30 && !switched; ++step) switched = tick().switched_;
        Check(switched && queue.Items().empty() && deck.Current().Title() == "Blue candidate",
              "serial GPU takeover commits matching row");
        tick(true, true);
        deck.ReleaseGraphics();
    }
    Check(renderer.Stats().texture_bytes_ == baseline, "serial GPU resources released");
    std::cout << "Serial GPU replacement: shared-budget rejection, retained red pixels, cancel "
                 "recovery, explicit retry and accepted blue pixels passed\n";
}
}  // namespace rhythm::validation
