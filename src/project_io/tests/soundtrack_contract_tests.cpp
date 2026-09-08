#include <chrono>
#include <fstream>
#include <iostream>
#include <limits>
#include <nlohmann/json.hpp>
#include <source_location>
#include <stdexcept>

#include "package_archive.h"
#include "rhythm/assets/store.h"
#include "rhythm/project/package.h"
#include "rhythm/project/store.h"

namespace {
void Check(bool value, std::source_location location = std::source_location::current()) {
    if (!value) throw std::runtime_error("soundtrack contract:" + std::to_string(location.line()));
}
template <typename Function>
void Reject(Function function) {
    bool rejected = false;
    try {
        function();
    } catch (const std::exception&) {
        rejected = true;
    }
    Check(rejected);
}
}  // namespace
int main(int argc, char* argv[]) {
    using namespace rhythm;
    try {
        if (argc != 2) throw std::invalid_argument("soundtrack_contract_tests output");
        const auto root =
                std::filesystem::path(argv[1]) /
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        std::filesystem::create_directories(root);
        const auto project_path = root / "show.rhythmproj";
        const auto source = root / "music.source";
        {
            std::ofstream file(source, std::ios::binary);
            file << "soundtrack structural fixture";
        }
        assets::Store store(project_path / "assets");
        const auto record = store.Import(source, "audio/wav");
        editor::Snapshot snapshot;
        graph::Registry registry;
        snapshot.document_.id_ = "soundtrack.contract";
        snapshot.document_.nodes_ = {registry.MakeNode(1, "texture.gradient"),
                                     registry.MakeNode(2, "output.texture")};
        snapshot.document_.edges_ = {{1, 1, 2, "source"}};
        snapshot.document_.output_ = 2;
        // Graph codecs retain their complete wire records as opaque extensions.
        snapshot.document_ = project::DecodeGraph(project::EncodeGraph(snapshot.document_));
        snapshot.assets_ = {record};
        snapshot.soundtrack_ = media::Soundtrack{record.id_, "作品音乐 / Music", 0.35F, true};
        project::Save(project_path, snapshot);
        Check(project::Load(project_path).snapshot_ == snapshot);
        auto changed = snapshot;
        changed.soundtrack_->gain_ = 0.8F;
        Reject([&] { project::Save(project_path, changed, project::CommitStep::kBeforeCommit); });
        Check(project::Load(project_path).snapshot_ == snapshot);
        editor::History history(snapshot);
        Check(history.Apply(changed, snapshot.document_.revision_));
        Check(history.Undo() && history.Current().soundtrack_ == snapshot.soundtrack_);
        Check(history.Redo() && history.Current().soundtrack_ == changed.soundtrack_);
        const std::vector<project::PackagedAsset> assets{{record, store.Read(record)}};
        const auto bytes = project::EncodePackage(snapshot.document_, snapshot.title_, assets,
                                                  snapshot.soundtrack_);
        const auto restored = project::DecodePackage(bytes);
        Check(restored.profile_ == project::PackageProfile::kMusicPerformanceV1 &&
              restored.soundtrack_ == snapshot.soundtrack_ && restored.assets_ == assets);
        Check(bytes == project::EncodePackage(snapshot.document_, snapshot.title_, assets,
                                              snapshot.soundtrack_));
        const auto entries = project::detail::ReadArchive(bytes);
        for (const auto& field : {"sha256", "gain", "loop"}) {
            auto bad = entries;
            auto metadata = nlohmann::json::parse(bad.at("manifest.json"));
            if (std::string_view(field) == "sha256")
                metadata["soundtrack"][field] = std::string(64, '0');
            if (std::string_view(field) == "gain") metadata["soundtrack"][field] = 2;
            if (std::string_view(field) == "loop") metadata["soundtrack"][field] = 1;
            bad["manifest.json"] = metadata.dump();
            Reject([&] { project::DecodePackage(project::detail::WriteArchive(bad)); });
        }
        auto bad = entries;
        auto metadata = nlohmann::json::parse(bad.at("manifest.json"));
        metadata["profile"] = "texture-signal-v2";
        bad["manifest.json"] = metadata.dump();
        Reject([&] { project::DecodePackage(project::detail::WriteArchive(bad)); });
        metadata["profile"] = "music-performance-v1";
        metadata.erase("soundtrack");
        bad["manifest.json"] = metadata.dump();
        Reject([&] { project::DecodePackage(project::detail::WriteArchive(bad)); });
        changed = snapshot;
        changed.assets_.clear();
        Reject([&] { project::Save(project_path, changed); });
        changed = snapshot;
        changed.soundtrack_->gain_ = std::numeric_limits<float>::quiet_NaN();
        Reject([&] { project::Save(project_path, changed); });
        changed = snapshot;
        changed.assets_[0].media_type_ = "image/png";
        Reject([&] { project::Save(project_path, changed); });
        auto arranged = snapshot;
        media::AudioClip clip{1, "Bass", record.id_};
        clip.timing_ = {0.25, 4, 0.1, 1.1, 1, parameters::ClipEnd::kLoop, 0.2, 0.3};
        auto second = clip;
        second.id_ = 2;
        second.title_ = "Echo";
        second.timing_.start_ = 2;
        second.pan_ = 0.5F;
        arranged.soundtrack_->clips_ = {clip, second};
        project::Save(project_path, arranged);
        Check(project::Load(project_path).snapshot_ == arranged);
        const auto arranged_bytes = project::EncodePackage(arranged.document_, arranged.title_,
                                                           assets, arranged.soundtrack_);
        const auto arranged_package = project::DecodePackage(arranged_bytes);
        Check(arranged_package.profile_ == project::PackageProfile::kMusicArrangementV1 &&
              arranged_package.soundtrack_ == arranged.soundtrack_);
        auto downgraded = project::detail::ReadArchive(arranged_bytes);
        auto arranged_metadata = nlohmann::json::parse(downgraded.at("manifest.json"));
        arranged_metadata["profile"] = "music-performance-v1";
        downgraded["manifest.json"] = arranged_metadata.dump();
        Reject([&] { project::DecodePackage(project::detail::WriteArchive(downgraded)); });
        arranged_metadata["profile"] = "music-arrangement-v1";
        arranged_metadata["soundtrack"]["clips"][1]["source_out"] = 0;
        downgraded["manifest.json"] = arranged_metadata.dump();
        Reject([&] { project::DecodePackage(project::detail::WriteArchive(downgraded)); });
        arranged.assets_[0].bytes_ = project::kMaximumPackageAssetBytes + 1;
        Reject([&] { project::RequiresStreamedAudio(arranged.assets_, arranged.soundtrack_); });
        snapshot.soundtrack_.reset();
        project::Save(project_path, snapshot);
        Check(!project::Load(project_path).snapshot_.soundtrack_);
        Check(!project::DecodePackage(project::EncodePackage(snapshot.document_, "old"))
                       .soundtrack_);
        std::cout << "soundtrack identity/settings, transaction, undo, profile and tamper checks "
                     "pass\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
