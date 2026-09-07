#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

#include "rhythm/assets/store.h"
#include "rhythm/media/audio_decoder.h"
#include "rhythm/player/package_loader.h"
#include "rhythm/player/session.h"
#include "rhythm/project/store.h"

namespace {
void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
}  // namespace
int main(int argc, char* argv[]) {
    using namespace rhythm;
    try {
        if (argc != 3) throw std::invalid_argument("soundtrack_player_tests music output");
        const auto root =
                std::filesystem::path(argv[2]) /
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        const auto asset_path = root / "work.rhythmproj" / "assets";
        assets::Store store(asset_path);
        const auto record =
                store.Import(argv[1], "audio/x-rhythm-media", project::kMaximumMusicAssetBytes);
        editor::Snapshot snapshot;
        graph::Registry registry;
        snapshot.title_ = "Portable music";
        snapshot.document_.id_ = "soundtrack.player";
        snapshot.document_.nodes_ = {registry.MakeNode(1, "texture.gradient"),
                                     registry.MakeNode(2, "output.texture")};
        snapshot.document_.edges_ = {{1, 1, 2, "source"}};
        snapshot.document_.output_ = 2;
        snapshot.assets_ = {record};
        snapshot.soundtrack_ = media::Soundtrack{record.id_, "音乐", 0.3F, true};
        project::Save(root / "work.rhythmproj", snapshot);
        const auto restored = project::Load(root / "work.rhythmproj").snapshot_;
        project::PublishSnapshot(root / "show.rhythmpack", restored, asset_path);
        const bool streamed = record.bytes_ > project::kMaximumPackageAssetBytes;
        const auto package = project::LoadPackage(root / "show.rhythmpack");
        Check(package.streamed_audio_.has_value() == streamed, "music profile selection");
        if (streamed) Check(package.assets_.empty(), "large music stays outside in-memory assets");
        std::string revision;
        std::getline(std::ifstream(root / "work.rhythmproj" / "CURRENT"), revision);
        const auto template_path = root / "template";
        std::filesystem::create_directories(template_path);
        for (const auto* name : {"graph.pb", "editor.json", "manifest.json"})
            std::filesystem::copy_file(root / "work.rhythmproj" / "revisions" / revision / name,
                                       template_path / name);
        assets::Store(template_path / "assets")
                .CopyFrom(store, record, project::kMaximumMusicAssetBytes);
        const auto copied = project::PrepareTemplate(template_path, root / "copied-assets");
        project::PublishSnapshot(root / "copied.rhythmpack", copied.snapshot_,
                                 root / "copied-assets");
        player::PackageLoader loader;
        Check(loader.StartFile(root / "copied.rhythmpack", root / "installed.rhythmpack"),
              "start file import");
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        std::optional<player::PackageLoadResult> loaded;
        while (!loaded && std::chrono::steady_clock::now() < deadline) {
            loaded = loader.Take();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        Check(loaded && loaded->error_ == player::PackageLoadError::kNone && loaded->package_,
              "verified file installation");
        player::Session session;
        session.LoadPrepared(std::move(*loaded->package_));
        const auto soundtrack = session.Soundtrack();
        Check(soundtrack && soundtrack->binding_ == *snapshot.soundtrack_, "portable settings");
        auto embedded = streamed ? media::AudioDecoder(soundtrack->file_bytes_)
                                 : media::AudioDecoder(soundtrack->bytes_);
        media::AudioDecoder original(argv[1]);
        for (;;) {
            const auto expected = original.Read();
            const auto actual = embedded.Read();
            Check(expected.has_value() == actual.has_value(), "portable exact duration");
            if (!expected) break;
            Check(expected->samples_ == actual->samples_, "portable exact decoded music");
        }
        // Invalid incoming music must fail before replacing a playing work.
        const std::string invalid = "not audio";
        const assets::AssetRecord bad{{project::Digest(invalid)}, invalid.size(), "audio/wav"};
        const std::vector<project::PackagedAsset> bad_assets{{bad, invalid}};
        auto binding = *snapshot.soundtrack_;
        binding.asset_ = bad.id_;
        bool rejected = false;
        try {
            session.Load(
                    project::EncodePackage(snapshot.document_, "Invalid", bad_assets, binding));
        } catch (const std::exception&) {
            rejected = true;
        }
        Check(rejected && session.Title() == snapshot.title_ &&
                      session.Soundtrack()->bytes_ == soundtrack->bytes_,
              "invalid soundtrack preserves current work");
        auto renderer = render::Renderer::CreateNull();
        renderer.BeginFrame();
        session.Tick(0, false, {64, 64}, renderer, {}, runtime::PlaybackSample{0.4, 12, false, 1});
        renderer.EndFrame();
        Check(session.Seconds() == 0.4, "portable performance follows music time");
        session.Load(project::EncodePackage(snapshot.document_, "No soundtrack"));
        Check(!session.Soundtrack(), "unbound package clears soundtrack");
        embedded.Seek(37, 3);
        Check(embedded.Read()->first_sample_ == 37,
              "old decoder retains source across package change");
        project::PublishPackage(root / "copied.rhythmpack", snapshot.document_, "Replacement");
        embedded.Seek(12000, 4);
        original.Seek(12000, 4);
        const auto retained = embedded.Read();
        const auto expected_tail = original.Read();
        Check(retained && expected_tail && retained->first_sample_ == 12000 &&
                      retained->samples_ == expected_tail->samples_,
              "old file music remains exact after its package is replaced");
        std::cout << "saved music -> package -> Player exact PCM, replacement and shared lifetime "
                     "pass\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
