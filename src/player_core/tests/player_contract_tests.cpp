#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "rhythm/player/session.h"

namespace {
void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}  // namespace
int main() {
    using namespace rhythm;
    try {
        graph::Registry registry;
        graph::Document document;
        document.id_ = "player.test";
        document.output_ = 3;
        document.canvas_ = {360, 640};
        document.nodes_ = {registry.MakeNode(1, "texture.gradient"),
                           registry.MakeNode(2, "texture.feedback"),
                           registry.MakeNode(3, "output.texture")};
        document.edges_ = {{1, 1, 2, "source"}, {2, 2, 3, "source"}};
        const auto bytes = project::EncodePackage(document, "测试播放");
        auto renderer = render::Renderer::CreateNull();
        player::Session session;
        Check(!session.Ready(), "empty session");
        session.Load(bytes);
        Check(session.Canvas() == render::Extent{360, 640},
              "Package canvas controls player aspect");
        const auto tick = [&](double seconds, bool suspended = false, render::Extent extent = {}) {
            if (!extent.width_) extent = session.Canvas();
            renderer.BeginFrame();
            auto frame = session.Tick(seconds, suspended, extent, renderer);
            renderer.EndFrame();
            return frame;
        };
        const auto first = tick(10);
        Check(renderer.IsValid(first.final_), "first output");
        tick(11);
        Check(session.Seconds() == 1, "running time");
        session.SetPaused(true);
        const auto paused = tick(12);
        const auto later = tick(40);
        Check(paused.final_ == later.final_ && renderer.Stats().passes_ == 0,
              "paused feedback must not advance");
        Check(session.Seconds() == 1, "paused clock");
        session.SetPaused(false);
        tick(50);
        tick(51);
        Check(session.Seconds() == 2, "resume without catch-up");
        Check(tick(52, true).final_ == render::TextureHandle{}, "no background draw");
        tick(100, true);
        session.ReleaseGraphics();
        Check(!renderer.IsValid(first.final_) && renderer.Stats().live_textures_ == 0,
              "surface release frees resources");
        const auto restored = tick(110);
        Check(session.Seconds() == 2 && renderer.IsValid(restored.final_), "surface restoration");
        bool rejected = false;
        try {
            session.Load("invalid package");
        } catch (const std::exception&) {
            rejected = true;
        }
        Check(rejected && session.Title() == "测试播放", "failed load preserves package");
        tick(111);
        Check(session.Seconds() == 3, "failed load preserves time");
        const auto resized = tick(112, false, {320, 180});
        Check(!renderer.IsValid(restored.final_) && renderer.IsValid(resized.final_), "resize");
        session.Restart();
        tick(120);
        Check(session.Seconds() == 0, "explicit restart");
        session.ReleaseGraphics();
        renderer.Invalidate();
        auto replacement = render::Renderer::CreateNull();
        replacement.BeginFrame();
        const auto replaced = session.Tick(121, false, {640, 360}, replacement);
        replacement.EndFrame();
        Check(replacement.IsValid(replaced.final_) && !renderer.IsValid(replaced.final_),
              "replacement device owns fresh handles");
        document.nodes_ = {registry.MakeNode(1, "participant.control"),
                           registry.MakeNode(2, "texture.gradient"),
                           registry.MakeNode(3, "output.texture")};
        document.edges_ = {{1, 1, 2, "amount"}, {2, 2, 3, "source"}};
        session.Load(project::EncodePackage(document, "external"));
        runtime::ExternalInputs inputs;
        inputs.participant_.controls_[0] = 0.75;
        const auto external_tick = [&](double time, render::Extent size = {360, 640}) {
            replacement.BeginFrame();
            auto result = session.Tick(time, false, size, replacement, inputs);
            replacement.EndFrame();
            return result;
        };
        const auto scalar = [](const runtime::FrameResult& result) {
            for (const auto& output : result.outputs_)
                if (output.node_ == 1) return output.scalar_;
            throw std::runtime_error("missing external input");
        };
        Check(scalar(external_tick(200)) == 0.75, "Player consumes typed external controls");
        session.SetPaused(true);
        inputs.participant_.controls_[0] = 0.25;
        Check(scalar(external_tick(201, {180, 320})) == 0.75, "pause resize holds snapshot");
        session.ReleaseGraphics();
        Check(scalar(external_tick(202)) == 0.75, "pause surface replacement holds snapshot");
        session.SetPaused(false);
        Check(scalar(external_tick(203)) == 0.25, "resume takes current snapshot");
        inputs.participant_.controls_[0] = std::numeric_limits<double>::quiet_NaN();
        rejected = false;
        const auto before_invalid = session.Seconds();
        try {
            session.Tick(204, false, session.Canvas(), replacement, inputs);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        Check(rejected && session.Seconds() == before_invalid,
              "invalid snapshot cannot advance clock");
        inputs = {};
        Check(scalar(external_tick(204)) == 0, "offline default clears previous controls");
        document.nodes_[0] = registry.MakeNode(1, "session.time");
        session.Load(project::EncodePackage(document, "session time"));
        inputs.session_seconds_ = 123;
        Check(scalar(external_tick(210)) == 123 && session.Seconds() == 0,
              "session time is distinct from local playback time");
        inputs.session_seconds_.reset();
        Check(scalar(external_tick(211)) == 1, "offline session time falls back to local clock");
        player::PreparedPackage prepared(bytes);
        Check(!prepared.SupportsAnalyticSeek() &&
                      prepared.Profile() == project::PackageProfile::kTextureSignalV2,
              "feedback package retains profile and requires history recovery");
        const auto digest = prepared.Digest();
        Check(prepared.Ready(), "worker preparation validates the package");
        auto transferred = std::move(prepared);
        Check(!prepared.Ready() && transferred.Ready() && transferred.Digest() == digest,
              "prepared ownership transfers exactly once");
        session.LoadPrepared(std::move(transferred));
        Check(!transferred.Ready() && session.Title() == "测试播放" && session.Seconds() == 0,
              "prepared load commits without decoding at the frame boundary");
        rejected = false;
        try {
            session.LoadPrepared(std::move(transferred));
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        Check(rejected && session.Title() == "测试播放",
              "empty prepared load preserves active scene");
        rejected = false;
        try {
            player::PreparedPackage invalid("invalid package");
        } catch (const std::exception&) {
            rejected = true;
        }
        Check(rejected && session.Title() == "测试播放",
              "failed worker preparation cannot replace active scene");
        rejected = false;
        try {
            session.LoadPrepared(player::PreparedPackage(bytes), 10);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        Check(rejected && session.Title() == "测试播放",
              "feedback late seek cannot replace active scene");
        player::PreparedPackage analytic(project::EncodePackage(document, "seek"));
        Check(analytic.SupportsAnalyticSeek(),
              "stateless graph can evaluate at a known scene time");
        session.LoadPrepared(std::move(analytic), 123);
        Check(scalar(external_tick(300)) == 123 && session.Seconds() == 123,
              "late stateless load begins at the scene offset");
        session.SetPaused(true);
        external_tick(400);
        session.ReleaseGraphics();
        Check(scalar(external_tick(401)) == 123,
              "seek offset survives pause and surface replacement");
        session.Restart();
        Check(scalar(external_tick(500)) == 0, "explicit restart clears seek offset");
        std::cout << "player contracts passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
