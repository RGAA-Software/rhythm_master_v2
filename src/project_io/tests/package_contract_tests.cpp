#include <filesystem>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>

#include "package_archive.h"
#include "rhythm/project/package.h"

namespace {
void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void Reject(std::string_view bytes) {
    bool rejected = false;
    try {
        rhythm::project::DecodePackage(bytes);
    } catch (const std::exception&) {
        rejected = true;
    }
    Check(rejected, "Invalid archive accepted");
}
}  // namespace

// Process-entry argument pointers are borrowed only for this synchronous ABI call.
int main(int argc, char* argv[]) {
    using namespace rhythm;
    try {
        graph::Registry registry;
        graph::Document document;
        document.id_ = "package.contract";
        document.revision_ = 1;
        document.output_ = 2;
        document.nodes_ = {registry.MakeNode(1, "texture.gradient"),
                           registry.MakeNode(2, "output.texture")};
        document.edges_ = {{1, 1, 2, "source"}};
        const auto bytes = project::EncodePackage(document, "渐变 / Gradient");
        Check(bytes == project::EncodePackage(document, "渐变 / Gradient"), "Reproducible archive");
        const auto decoded = project::DecodePackage(bytes);
        Check(decoded.profile_ == project::PackageProfile::kTextureSignalV2,
              "Decoded current profile retained");
        Check(decoded.title_ == "渐变 / Gradient" && decoded.program_.revision_ == 1 &&
                      decoded.program_.document_id_ == document.id_,
              "Package identity");
        const auto entries = project::detail::ReadArchive(bytes);
        Check(entries.size() == 2 && entries.contains("runtime/program.pb"),
              "Compiled archive layout");
        const project::PackagedAsset asset{
                {{"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"},
                 3,
                 "application/octet-stream"},
                "abc"};
        const std::vector<project::PackagedAsset> assets{asset};
        const auto bundled = project::EncodePackage(document, "With asset", assets);
        Check(project::DecodePackage(bundled).assets_ == assets, "Portable asset bytes round trip");
        Check(bundled == project::EncodePackage(document, "With asset", assets),
              "Reproducible asset package");
        const auto bundled_entries = project::detail::ReadArchive(bundled);
        auto corrupt_asset = bundled_entries;
        corrupt_asset["assets/" + asset.record_.id_.sha256_] = "abd";
        Reject(project::detail::WriteArchive(corrupt_asset));
        corrupt_asset = bundled_entries;
        corrupt_asset.erase("assets/" + asset.record_.id_.sha256_);
        Reject(project::detail::WriteArchive(corrupt_asset));
        corrupt_asset = entries;
        corrupt_asset["assets/" + asset.record_.id_.sha256_] = "abc";
        Reject(project::detail::WriteArchive(corrupt_asset));
        corrupt_asset = bundled_entries;
        auto asset_metadata = nlohmann::json::parse(corrupt_asset["manifest.json"]);
        asset_metadata["assets"].push_back(asset_metadata["assets"][0]);
        corrupt_asset["manifest.json"] = asset_metadata.dump();
        Reject(project::detail::WriteArchive(corrupt_asset));
        corrupt_asset = bundled_entries;
        corrupt_asset["assets/" + asset.record_.id_.sha256_].assign(
                project::kMaximumPackageAssetBytes + 1, 'x');
        Reject(project::detail::WriteArchive(corrupt_asset));
        bool duplicate_rejected = false;
        try {
            project::EncodePackage(document, "Duplicate", std::vector{asset, asset});
        } catch (const std::invalid_argument&) {
            duplicate_rejected = true;
        }
        Check(duplicate_rejected, "Publisher rejects duplicate asset identity");
        auto bad = entries;
        bad["runtime/program.pb"][0] ^= 1;
        Reject(project::detail::WriteArchive(bad));
        bad = entries;
        bad["../escape"] = bad.extract("manifest.json").mapped();
        Reject(project::detail::WriteArchive(bad));
        bad = entries;
        bad["runtime/program.pb"].assign(project::kMaximumProgramBytes + 1, 'x');
        Reject(project::detail::WriteArchive(bad));
        bad = entries;
        bad["manifest.json"].insert(1, "\"profile\":\"texture-signal-v1\",");
        Reject(project::detail::WriteArchive(bad));
        bad = entries;
        auto metadata = nlohmann::json::parse(bad["manifest.json"]);
        metadata["program_abi"] = 999;
        bad["manifest.json"] = metadata.dump();
        Reject(project::detail::WriteArchive(bad));
        bad = entries;
        bad["source/graph.pb"] = "unexpected";
        Reject(project::detail::WriteArchive(bad));
        Reject(bytes.substr(0, bytes.size() / 2));
        Reject(std::string(project::kMaximumPackageBytes + 1, 'x'));

        const auto root =
                argc >= 2 ? std::filesystem::path(argv[1]) : std::filesystem::path("package-tests");
        if (argc >= 3) {
            const auto legacy = project::LoadPackage(argv[2]);
            Check(legacy.profile_ == project::PackageProfile::kTextureSignalV1,
                  "Decoded legacy profile retained");
            Check(legacy.program_.canvas_ == graph::Canvas{} &&
                          !legacy.program_.instructions_.empty(),
                  "Legacy ABI 1 package retains its default canvas");
        }
        document.canvas_ = {720, 1280};
        const auto portrait = project::DecodePackage(project::EncodePackage(document, "Portrait"));
        Check(portrait.program_.canvas_ == document.canvas_, "Portrait canvas package round trip");
        auto wrong_canvas =
                project::detail::ReadArchive(project::EncodePackage(document, "Portrait"));
        auto canvas_metadata = nlohmann::json::parse(wrong_canvas["manifest.json"]);
        canvas_metadata["canvas"]["height"] = 720;
        wrong_canvas["manifest.json"] = canvas_metadata.dump();
        Reject(project::detail::WriteArchive(wrong_canvas));
        const auto path = root / std::filesystem::path(u8"发布.rhythmpack");
        project::PublishPackage(path, document, "First");
        document.revision_ = 2;
        bool injected = false;
        try {
            project::PublishPackage(path, document, "Second", true);
        } catch (const std::runtime_error&) {
            injected = true;
        }
        Check(injected && project::LoadPackage(path).program_.revision_ == 1,
              "Failed publish preserves old package");
        project::PublishPackage(path, document, "Second");
        Check(project::LoadPackage(path).program_.revision_ == 2,
              "Atomic replacement publishes the next revision");
        bool invalid_install = false;
        try {
            project::InstallPackage(path, "invalid incoming content");
        } catch (const std::exception&) {
            invalid_install = true;
        }
        Check(invalid_install && project::LoadPackage(path).program_.revision_ == 2,
              "Invalid import retains installed package");
        project::InstallPackage(path, bytes);
        Check(project::LoadPackage(path).program_.revision_ == 1,
              "Validated imported bytes install atomically");
        project::InstallPackage(path, bundled);
        Check(project::LoadPackage(path).assets_ == assets,
              "Installed assets remain self-contained");
        for (const auto& entry : std::filesystem::directory_iterator(root))
            Check(entry.path().extension() != ".tmp", "Staging files must be released");
        std::cout << "package contracts passed: ZIP, hash, limits, metadata, atomic replacement\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
