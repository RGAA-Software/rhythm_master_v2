#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <thread>

#include "rhythm/assets/store.h"
#include "rhythm/player/session.h"
#include "rhythm/project/store.h"

namespace {
void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}  // namespace
int main(int argc, char** argv) {
    using namespace rhythm;
    try {
        Check(argc == 3, "video fixtures and scratch directories required");
        const std::filesystem::path directory(argv[2]);
        assets::Store store(directory / "video.rhythmproj" / "assets");
        const auto record =
                store.Import(std::filesystem::path(argv[1]) / std::filesystem::path(u8"视频.mkv"),
                             "video/x-matroska");
        const std::vector<project::PackagedAsset> assets{{record, store.Read(record)}};
        graph::Registry registry;
        editor::Snapshot snapshot;
        snapshot.title_ = "Two independent videos";
        snapshot.assets_ = {record};
        auto& document = snapshot.document_;
        document.id_ = "video.player";
        document.canvas_ = {128, 128};
        document.nodes_ = {
                registry.MakeNode(1, "texture.video"), registry.MakeNode(2, "texture.video"),
                registry.MakeNode(3, "texture.blend"), registry.MakeNode(4, "output.texture")};
        document.nodes_[0].properties_["asset"] = record.id_;
        document.nodes_[1].properties_["asset"] = record.id_;
        document.nodes_[1].properties_["video_speed"] = 0.0;
        document.nodes_[1].properties_["video_offset"] = 0.8;
        document.edges_ = {{1, 1, 3, "a"}, {2, 2, 3, "b"}, {3, 3, 4, "source"}};
        document.output_ = 4;
        const auto plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
        const auto prepared = prepared_assets::Prepare(plan, assets);
        Check(prepared->videos_.size() == 1 && prepared_assets::Covers(plan, *prepared),
              "duplicate consumers share verified encoded media");
        auto over_budget = plan;
        for (int index = 0; index < 3; ++index) {
            auto instruction = plan.instructions_[0];
            instruction.node_.id_ = 100 + index;
            over_budget.instructions_.push_back(std::move(instruction));
        }
        bool rejected = false;
        try {
            prepared_assets::Prepare(over_budget, assets);
        } catch (const std::length_error&) {
            rejected = true;
        }
        Check(rejected && !prepared_assets::Covers(over_budget, *prepared), "four-instance budget");
        video_sources::Streams streams;
        auto renderer = render::Renderer::CreateNull();
        runtime::Runtime runtime;
        auto await = [&](double time, std::uint64_t generation, double expected_first) {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
            for (;;) {
                auto samples = streams.Update(plan, *prepared, time, generation);
                Check(streams.Error().empty(), "video worker error");
                bool complete = samples.size() == 2;
                for (const auto& sample : samples) {
                    const auto expected = sample.node_ == 1 ? expected_first : 0.8;
                    complete &= std::abs(sample.frame_->seconds_ - expected) < 0.001;
                    Check(sample.generation_ == generation, "stale generation supplied to runtime");
                }
                if (complete) return samples;
                Check(std::chrono::steady_clock::now() < deadline, "video node worker deadline");
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        };
        auto evaluate = [&](double time, std::uint64_t generation, double expected_first) {
            runtime::FrameContext context{time, generation, {128, 128}, true};
            context.videos_ = await(time, generation, expected_first);
            renderer.BeginFrame();
            const auto frame = runtime.Evaluate(plan, context, renderer);
            renderer.EndFrame();
            Check(renderer.IsValid(frame.final_), "video graph output exists");
            return frame;
        };
        evaluate(0, 1, 0);
        const auto bytes = renderer.Stats().texture_bytes_;
        for (int index = 1; index < 10; ++index) {
            evaluate(index * 0.2, 1, index * 0.2);
            Check(renderer.Stats().texture_bytes_ == bytes,
                  "video uploads reuse fixed GPU allocation");
        }
        Check(evaluate(1.8, 1, 1.8).evaluated_ == 0, "paused video output stays cached");
        evaluate(2.21, 1, 0.2);
        evaluate(0.4, 2, 0.4);
        runtime.Reset();
        Check(renderer.Stats().texture_bytes_ == 0, "video resource release");
        evaluate(0.4, 2, 0.4);
        project::Save(directory / "video.rhythmproj", snapshot);
        const auto reopened = project::Load(directory / "video.rhythmproj");
        project::PublishSnapshot(directory / "video.rhythmpack", reopened.snapshot_,
                                 directory / "video.rhythmproj" / "assets");
        player::Session session;
        session.Open(directory / "video.rhythmpack");
        session.SetPaused(true);
        for (int index = 0; index < 20; ++index) {
            renderer.BeginFrame();
            Check(renderer.IsValid(session.Tick(0, false, {128, 128}, renderer).final_),
                  "paused Player accepts arriving decoded frames");
            renderer.EndFrame();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        session.ReleaseGraphics();
        renderer.BeginFrame();
        Check(renderer.IsValid(session.Tick(0, false, {128, 128}, renderer).final_),
              "Player video device recreation");
        renderer.EndFrame();
        std::cout << "Video nodes: independent source time, seek/loop, cache, bounded uploads, "
                     "save/publish and Player passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
