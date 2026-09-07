#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

#include "rhythm/assets/store.h"
#include "rhythm/player/session.h"
#include "rhythm/prepared_assets/loader.h"
#include "rhythm/project/store.h"

namespace {
void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
template <typename F>
void Reject(F action) {
    bool rejected = false;
    try {
        action();
    } catch (const std::exception&) {
        rejected = true;
    }
    Check(rejected, "invalid image accepted");
}
}  // namespace
int main(int argc, char** argv) {
    using namespace rhythm;
    try {
        Check(argc == 3, "image fixture and scratch paths required");
        const std::filesystem::path directory(argv[2]);
        assets::Store store(directory / "image.rhythmproj" / "assets");
        const auto record = store.Import(argv[1], "image/png");
        const std::vector<project::PackagedAsset> assets{{record, store.Read(record)}};
        graph::Registry registry;
        editor::Snapshot snapshot;
        snapshot.title_ = "Embedded image";
        auto& document = snapshot.document_;
        document.id_ = "image.player";
        document.canvas_ = {128, 128};
        document.nodes_ = {registry.MakeNode(1, "texture.image"),
                           registry.MakeNode(2, "output.texture")};
        document.nodes_[0].properties_["asset"] = record.id_;
        document.edges_ = {{1, 1, 2, "source"}};
        document.output_ = 2;
        snapshot.assets_ = {record};
        const auto plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
        const auto resources = prepared_assets::Prepare(plan, assets);
        const auto& image = resources->images_->images_.at(0);
        Check(image.width_ == 64 && image.height_ == 48 && image.rgba_.size() == 64 * 48 * 4 &&
                      image.rgba_[0] == 120 && image.rgba_[1] == 40 && image.rgba_[2] == 200 &&
                      image.rgba_[3] == 64,
              "embedded PNG preserves straight alpha and exact channels");
        Check(prepared_assets::Covers(plan, *resources) && !prepared_assets::Covers(plan, {}),
              "resource coverage includes image IDs");
        auto duplicate = plan;
        duplicate.instructions_.push_back(plan.instructions_[0]);
        Check(prepared_assets::Prepare(duplicate, assets)->images_->images_.size() == 1,
              "shared source decoded once");
        Reject([&] { prepared_assets::Prepare(plan, {}); });
        auto corrupt = assets;
        corrupt[0].bytes_[0] ^= 1;
        Reject([&] { prepared_assets::Prepare(plan, corrupt); });
        corrupt = assets;
        corrupt[0].record_.media_type_ = "video/mp4";
        Reject([&] { prepared_assets::Prepare(plan, corrupt); });
        std::stop_source stop;
        stop.request_stop();
        Reject([&] { prepared_assets::Prepare(plan, assets, stop.get_token()); });
        prepared_assets::Loader loader;
        loader.Submit({plan, {record}, directory / "image.rhythmproj" / "assets", 1});
        loader.Submit({plan, {record}, directory / "image.rhythmproj" / "assets", 2});
        std::optional<prepared_assets::LoadResult> loaded;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (!loaded && std::chrono::steady_clock::now() < deadline) {
            loaded = loader.Take();
            if (!loaded) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        Check(loaded && loaded->generation_ == 2 && loaded->error_.empty() &&
                      prepared_assets::Covers(plan, *loaded->resources_),
              "worker commits the latest complete image resource set");
        project::Save(directory / "image.rhythmproj", snapshot);
        const auto reopened = project::Load(directory / "image.rhythmproj");
        project::PublishSnapshot(directory / "image.rhythmpack", reopened.snapshot_,
                                 directory / "image.rhythmproj" / "assets");
        player::Session session;
        session.Open(directory / "image.rhythmpack");
        auto renderer = render::Renderer::CreateNull();
        auto tick = [&] {
            renderer.BeginFrame();
            const auto frame = session.Tick(0, false, {128, 128}, renderer);
            renderer.EndFrame();
            return frame;
        };
        const auto first = tick();
        Check(renderer.IsValid(first.final_), "published image reaches player rendering");
        Reject([&] { session.Load(project::EncodePackage(document, "bad MIME", corrupt)); });
        Check(session.Title() == "Embedded image" && renderer.IsValid(first.final_),
              "failed replacement preserves active package and frame");
        session.ReleaseGraphics();
        Check(renderer.Stats().texture_bytes_ == 0, "device release frees image resources");
        Check(renderer.IsValid(tick().final_), "device recreation reuses immutable CPU pixels");
        std::cout << "Image import, worker, save, publish, player and device recreation passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
