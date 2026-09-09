#include <jni.h>

#include <iostream>
#include <source_location>
#include <stdexcept>

#include "../control_bridge.h"
#include "../control_bridge_state.h"

namespace rhythm::android_host {
extern "C" jboolean Java_org_rhythmmaster_player_PerformanceControls_nativeValue(JNIEnv*, jclass,
                                                                                 jlong, jlong,
                                                                                 jdouble);
extern "C" jboolean Java_org_rhythmmaster_player_PerformanceControls_nativeRecall(JNIEnv*, jclass,
                                                                                  jlong, jlong);
extern "C" jboolean Java_org_rhythmmaster_player_PerformanceControls_nativeFollow(JNIEnv*, jclass,
                                                                                  jlong);
extern "C" jboolean Java_org_rhythmmaster_player_BeatControls_nativeGrid(JNIEnv*, jclass, jlong,
                                                                         jboolean, jdouble, jint,
                                                                         jint, jdouble);
extern "C" jboolean Java_org_rhythmmaster_player_BeatControls_nativeMode(JNIEnv*, jclass, jlong,
                                                                         jint);
extern "C" jdouble Java_org_rhythmmaster_player_BeatControls_nativeTap(JNIEnv*, jclass, jlong,
                                                                       jdouble);
extern "C" jboolean Java_org_rhythmmaster_player_BeatControls_nativeCancel(JNIEnv*, jclass, jlong,
                                                                           jlong);
}  // namespace rhythm::android_host
namespace {
void Check(bool value, std::source_location location = std::source_location::current()) {
    if (!value)
        throw std::runtime_error("android.control_bridge:" + std::to_string(location.line()));
}
}  // namespace
int main() {
    using namespace rhythm;
    using namespace rhythm::android_host;
    try {
        graph::Registry registry;
        graph::Document document;
        document.id_ = "bridge-test";
        document.canvas_ = {32, 32};
        document.nodes_ = {registry.MakeNode(1, "control.scalar"),
                           registry.MakeNode(2, "texture.gradient"),
                           registry.MakeNode(3, "output.texture")};
        document.nodes_[0].properties_["value"] = 0.2;
        document.edges_ = {{1, 1, 2, "amount"}, {2, 2, 3, "source"}};
        document.output_ = 3;
        document.control_snapshots_ = {{1, "Quiet", {{1, 0.1}}}, {2, "Bright", {{1, 0.9}}}};
        document.beat_grid_ = parameters::BeatSettings{};
        const auto bytes = project::EncodePackage(document, "Bridge");
        player::SceneDeck deck;
        deck.LoadPrepared(player::PreparedPackage(bytes));
        PublishControls(deck);
        auto renderer = render::Renderer::CreateNull();
        double monotonic = 0;
        const auto tick = [&](double seconds, bool paused = false) {
            renderer.BeginFrame();
            const auto result =
                    deck.Tick(monotonic += 0.1, false, player::RenderQuality::kOriginal, renderer,
                              {}, runtime::PlaybackSample{seconds, 1, paused});
            Check(renderer.IsValid(result.output_.final_));
            renderer.EndFrame();
            PublishControlFrame(deck);
        };
        tick(0.1);
        const auto version = static_cast<jlong>(detail::ControlBridge().generation_);
        // These primitive JNI entries deliberately do not access env/class.
        Check(Java_org_rhythmmaster_player_BeatControls_nativeMode(nullptr, nullptr, version, 1));
        Check(Java_org_rhythmmaster_player_PerformanceControls_nativeRecall(nullptr, nullptr,
                                                                            version, 2));
        Check(deck.ActionStatus(player::PerformanceActionKind::kSnapshot).state_ ==
              player::PerformanceActionState::kIdle);
        ApplyControlCommands(deck);
        tick(0.49);
        Check(deck.Current().CurrentControls().at(1) == 0.2);
        tick(0.5, true);
        Check(deck.Current().CurrentControls().at(1) == 0.2);
        tick(0.5);
        Check(deck.Current().CurrentControls().at(1) == 0.9);
        Check(Java_org_rhythmmaster_player_PerformanceControls_nativeValue(nullptr, nullptr,
                                                                           version, 1, 0.4));
        Check(deck.Current().CurrentControls().at(1) == 0.9);
        ApplyControlCommands(deck);
        tick(0.6);
        Check(deck.Current().CurrentControls().at(1) == 0.4 && deck.HasControlOverrides());
        Check(Java_org_rhythmmaster_player_PerformanceControls_nativeFollow(nullptr, nullptr,
                                                                            version));
        ApplyControlCommands(deck);
        tick(0.7);
        Check(deck.Current().CurrentControls().at(1) == 0.2 && !deck.HasControlOverrides());
        Check(Java_org_rhythmmaster_player_PerformanceControls_nativeRecall(nullptr, nullptr,
                                                                            version, 1));
        ApplyControlCommands(deck);
        tick(0.8);
        const auto pending = deck.ActionStatus(player::PerformanceActionKind::kSnapshot).id_;
        Check(Java_org_rhythmmaster_player_BeatControls_nativeCancel(nullptr, nullptr, version,
                                                                     static_cast<jlong>(pending)));
        ApplyControlCommands(deck);
        tick(1.1);
        Check(deck.Current().CurrentControls().at(1) == 0.2);
        Check(deck.ActionStatus(player::PerformanceActionKind::kSnapshot).state_ ==
              player::PerformanceActionState::kCancelled);
        Check(!Java_org_rhythmmaster_player_PerformanceControls_nativeRecall(nullptr, nullptr,
                                                                             version, 999));
        Check(!Java_org_rhythmmaster_player_PerformanceControls_nativeValue(nullptr, nullptr,
                                                                            version, 999, 0.4));
        Check(!Java_org_rhythmmaster_player_BeatControls_nativeGrid(nullptr, nullptr, version, true,
                                                                    0, 4, 4, 0));
        Check(Java_org_rhythmmaster_player_BeatControls_nativeGrid(nullptr, nullptr, version, true,
                                                                   97, 6, 8, -0.25));
        ApplyControlCommands(deck);
        tick(1.2);
        Check(deck.BeatGrid() == parameters::BeatSettings{97, 6, 8, -0.25});
        for (int tap = 0; tap < 4; ++tap) {
            const auto bpm = Java_org_rhythmmaster_player_BeatControls_nativeTap(
                    nullptr, nullptr, version, tap * 0.25);
            Check(bpm == (tap == 3 ? 120 : 0));
        }
        ApplyControlCommands(deck);
        Check(deck.BeatGrid()->bpm_ == 120);
        Check(Java_org_rhythmmaster_player_BeatControls_nativeGrid(nullptr, nullptr, version, false,
                                                                   0, 0, 0, 0));
        Check(!Java_org_rhythmmaster_player_BeatControls_nativeMode(nullptr, nullptr, version, 1));
        ApplyControlCommands(deck);
        Check(!deck.BeatGrid() && CurrentQuantization() == parameters::Quantization::kImmediate);
        Check(Java_org_rhythmmaster_player_PerformanceControls_nativeValue(nullptr, nullptr,
                                                                           version, 1, 0.8));
        deck.LoadPrepared(player::PreparedPackage(bytes));
        PublishControls(deck);
        Check(!Java_org_rhythmmaster_player_PerformanceControls_nativeRecall(nullptr, nullptr,
                                                                             version, 1));
        Check(!Java_org_rhythmmaster_player_BeatControls_nativeMode(nullptr, nullptr, version, 1));
        ApplyControlCommands(deck);
        tick(0);
        Check(!deck.HasControlOverrides() && deck.Current().CurrentControls().at(1) == 0.2);
        std::cout << "Android control bridge: queued JNI values, quantized recall, pause, cancel, "
                     "grid/taps, validation and stale work generation passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
