#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "rhythm/player/render_quality.h"
#include "rhythm/player/session.h"

namespace {
void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void VerifySurfacePackage(const std::filesystem::path& path) {
    using namespace rhythm;
    player::Session session;
    session.Open(path);
    auto renderer = render::Renderer::CreateNull();
    renderer.BeginFrame();
    const auto first = session.Tick(0, false, {64, 64}, renderer);
    renderer.EndFrame();
    Check(renderer.IsValid(first.final_) && renderer.Stats().surface_programs_ == 1,
          "published surface program reaches Player output");
    session.ReleaseGraphics();
    Check(renderer.Stats().surface_programs_ == 0 && !renderer.IsValid(first.final_),
          "Player release frees surface program and old output");
    runtime::PreparationProgress prepared;
    for (unsigned step = 0; step < 32; ++step) {
        renderer.BeginFrame();
        prepared =
                session.PrepareGraphics(step + 1, {64, 64}, renderer, {}, {0, 42, true}, {1, 100});
        renderer.EndFrame();
        Check(prepared.state_ != runtime::PreparationState::kFailed,
              "surface candidate preparation");
        if (prepared.state_ == runtime::PreparationState::kReady) break;
    }
    Check(prepared.state_ == runtime::PreparationState::kReady && prepared.output_ &&
                  renderer.IsValid(prepared.output_->final_) &&
                  renderer.Stats().surface_programs_ == 1,
          "staged Player recreates surface shader");
    renderer.BeginFrame();
    const auto paused =
            session.Tick(40, false, {64, 64}, renderer, {}, runtime::PlaybackSample{0, 42, true});
    renderer.EndFrame();
    Check(paused.final_ == prepared.output_->final_ && renderer.Stats().passes_ == 0,
          "paused Player reuses prepared surface frame");
    renderer.BeginFrame();
    const auto live =
            session.Tick(41, false, {64, 64}, renderer, {}, runtime::PlaybackSample{1, 42, false});
    renderer.EndFrame();
    Check(renderer.IsValid(live.final_) && renderer.Stats().passes_ > 0 &&
                  renderer.Stats().surface_programs_ == 1,
          "surface time resumes without recompiling a program");
    session.ReleaseGraphics();
    Check(renderer.Stats().surface_programs_ == 0, "surface program is released deterministically");
    std::cout
            << "Surface Player: package, staged preparation, pause/resume and recreation passed\n";
}
void VerifyPreparation() {
    using namespace rhythm;
    graph::Registry registry;
    graph::Document document;
    document.id_ = "session.preparation";
    document.canvas_ = {32, 32};
    document.nodes_ = {registry.MakeNode(1, "core.time"), registry.MakeNode(2, "texture.gradient")};
    document.edges_ = {{1, 1, 2, "amount"}};
    for (std::uint64_t id = 3; id <= 24; ++id) {
        document.nodes_.push_back(registry.MakeNode(id, "texture.transform"));
        document.edges_.push_back({id, id - 1, id, "source"});
    }
    document.nodes_.push_back(registry.MakeNode(25, "output.texture"));
    document.edges_.push_back({25, 24, 25, "source"});
    document.output_ = 25;
    player::Session session;
    session.Load(project::EncodePackage(document, "Prepared"));
    auto renderer = render::Renderer::CreateNull();
    runtime::PreparationProgress prepared;
    for (unsigned step = 0; step < 25; ++step) {
        renderer.BeginFrame();
        prepared = session.PrepareGraphics(step, {32, 32}, renderer, {}, {0, 42, true}, {1, 100});
        renderer.EndFrame();
        Check(prepared.state_ != runtime::PreparationState::kFailed && session.Seconds() == 0 &&
                      prepared.completed_nodes_ == step + 1,
              "candidate preparation holds time and advances bounded steps");
        if (step != 24) Check(!prepared.output_, "candidate partial output remains private");
    }
    Check(prepared.state_ == runtime::PreparationState::kReady, "candidate is presentable");
    renderer.BeginFrame();
    const auto first =
            session.Tick(25, false, {32, 32}, renderer, {}, runtime::PlaybackSample{0, 42, true});
    renderer.EndFrame();
    Check(first.final_ == prepared.output_->final_ && renderer.Stats().passes_ == 0,
          "first paused presentation reuses prepared GPU state across clock binding");
    renderer.BeginFrame();
    const auto live = session.Tick(26, false, {32, 32}, renderer, {},
                                   runtime::PlaybackSample{0.5, 42, false});
    renderer.EndFrame();
    Check(renderer.IsValid(live.final_) && live.outputs_.front().scalar_ == 0.5,
          "accepted candidate advances from its scene-local clock");
    session.ReleaseGraphics();
    renderer.BeginFrame();
    session.PrepareGraphics(27, {32, 32}, renderer, {}, {0, 43, true}, {3, 100});
    renderer.EndFrame();
    session.ReleaseGraphics();
    Check(renderer.Stats().texture_bytes_ == 0, "cancel releases partial candidate resources");
}
}  // namespace
int main(int argc, char* argv[]) {
    using namespace rhythm;
    try {
        if (argc == 2)
            VerifySurfacePackage(argv[1]);
        else
            Check(argc == 1, "optional surface package path");
        VerifyPreparation();
        using player::PlaybackExtent;
        using player::RenderQuality;
        Check(PlaybackExtent({1280, 720}, RenderQuality::kBalanced) == render::Extent{960, 540},
              "balanced landscape budget");
        Check(PlaybackExtent({720, 1280}, RenderQuality::kBalanced) == render::Extent{540, 960},
              "balanced portrait preserves orientation");
        Check(PlaybackExtent({1024, 1024}, RenderQuality::kBalanced) == render::Extent{720, 720},
              "balanced square pixel budget");
        Check(PlaybackExtent({1280, 720}, RenderQuality::kEconomy) == render::Extent{640, 360},
              "economy budget");
        Check(PlaybackExtent({320, 180}, RenderQuality::kEconomy) == render::Extent{320, 180},
              "quality never upscales");
        Check(PlaybackExtent({720, 1280}, RenderQuality::kOriginal) == render::Extent{720, 1280},
              "original quality preserves canvas");
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
        runtime::PlaybackSample music{4.25, 7, false, 16};
        const auto music_tick = [&](double host_seconds) {
            replacement.BeginFrame();
            auto result =
                    session.Tick(host_seconds, false, session.Canvas(), replacement, {}, music);
            replacement.EndFrame();
            return result;
        };
        Check(scalar(music_tick(501)) == 4.25 && session.Seconds() == 4.25,
              "music consumption drives graph local and session fallback time");
        music.paused_ = true;
        const auto held = music_tick(502);
        Check(session.Paused() && scalar(music_tick(600)) == 4.25,
              "media pause holds the player without wall-time drift");
        music.seconds_ = 9;
        ++music.generation_;
        const auto sought_music = music_tick(601);
        Check(scalar(sought_music) == 9 && !replacement.IsValid(held.final_),
              "paused media seek reevaluates graph and invalidates prior history");
        music.seconds_ = 0;
        music.paused_ = false;
        ++music.generation_;
        Check(scalar(music_tick(602)) == 0 && !session.Paused(), "whole-song loop resets graph");
        session.Load(bytes);
        music.seconds_ = 2;
        ++music.generation_;
        const auto history = music_tick(603);
        music.paused_ = true;
        music.seconds_ = 8;
        ++music.generation_;
        Check(replacement.IsValid(music_tick(604).final_) && !replacement.IsValid(history.final_) &&
                      session.Seconds() == 8,
              "interactive stateful seek starts fresh history instead of rejecting audio seek");
        std::cout << "player contracts passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
