#include <google/protobuf/unknown_field_set.h>

#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <source_location>
#include <stdexcept>
#include <thread>

#include "graph.pb.h"
#include "rhythm/assets/store.h"
#include "rhythm/graph/compiler.h"
#include "rhythm/project/async_store.h"
#include "rhythm/project/package.h"
#include "rhythm/project/store.h"
#include "rhythm/storage/atomic_file.h"

namespace {
void Check(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition)
        throw std::runtime_error("persistence.contract:" + std::to_string(location.line()));
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
        if (argc != 3) throw std::invalid_argument("test.arguments");
        const auto initial = project::LoadRevision(argv[1]).snapshot_;
        Check(initial.document_.nodes_.size() == 8);
        {
            graph::Registry registry;
            graph::Document named;
            named.id_ = "binding.roundtrip";
            named.nodes_ = {registry.MakeNode(1, "scalar.constant"),
                            registry.MakeNode(2, "texture.gradient"),
                            registry.MakeNode(3, "output.texture")};
            named.output_ = 3;
            named.edges_ = {{1, 2, 3, "source"}};
            named.signals_ = {{"brightness", 1}};
            named.bindings_ = {{2, "amount", "brightness"}};
            schema::GraphProject message;
            Check(message.ParseFromString(project::EncodeGraph(named)));
            Check(message.schema_version() == 3);
            auto& signal = *message.mutable_signals(0);
            signal.GetReflection()->MutableUnknownFields(&signal)->AddVarint(100, 77);
            const auto restored = project::DecodeGraph(message.SerializeAsString());
            Check(restored.signals_[0].source_ == 1 &&
                  restored.bindings_[0].signal_ == "brightness");
            Check(message.ParseFromString(project::EncodeGraph(restored)));
            Check(message.signals(0)
                          .GetReflection()
                          ->GetUnknownFields(message.signals(0))
                          .field_count() == 1);
            const auto plan = std::get<graph::ExecutionPlan>(graph::Compile(restored, registry));
            const auto loaded = project::DecodeProgram(project::EncodeProgram(plan));
            Check(loaded.instructions_.size() == 3 && loaded.instructions_[1].inputs_[0] == 0);
            message.set_schema_version(2);
            Reject([&] { project::DecodeGraph(message.SerializeAsString()); });
        }
        {
            auto curve_document = initial.document_;
            curve_document.nodes_.push_back(graph::Registry{}.MakeNode(100, "scalar.curve"));
            schema::GraphProject curve_message;
            Check(curve_message.ParseFromString(project::EncodeGraph(curve_document)));
            auto& curve = *(*curve_message.mutable_nodes(8)->mutable_properties())["curve"]
                                   .mutable_curve();
            auto& key = *curve.mutable_keys(0);
            key.set_interpolation(schema::Curve::INTERPOLATION_HERMITE);
            key.set_in_slope(-2);
            key.set_out_slope(3);
            key.GetReflection()->MutableUnknownFields(&key)->AddVarint(100, 99);
            const auto decoded_curve = project::DecodeGraph(curve_message.SerializeAsString());
            Check(curve_message.ParseFromString(project::EncodeGraph(decoded_curve)));
            const auto& retained = curve_message.nodes(8).properties().at("curve").curve().keys(0);
            Check(retained.GetReflection()->GetUnknownFields(retained).field_count() == 1);
            Check(retained.interpolation() == schema::Curve::INTERPOLATION_HERMITE &&
                  retained.in_slope() == -2 && retained.out_slope() == 3);
            auto& invalid = *(*curve_message.mutable_nodes(8)->mutable_properties())["curve"]
                                     .mutable_curve();
            invalid.mutable_keys(1)->set_seconds(0);
            Reject([&] { project::DecodeGraph(curve_message.SerializeAsString()); });
        }
        Check(project::Digest("abc") ==
              "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
        const auto project_path =
                std::filesystem::path(argv[2]) / std::filesystem::path(u8"恢复测试.rhythmproj");
        project::Save(project_path, initial);
        {
            const auto asset_project = std::filesystem::path(argv[2]) / "with-assets.rhythmproj";
            const auto source = std::filesystem::path(argv[2]) / "asset-source.bin";
            {
                std::ofstream file(source, std::ios::binary);
                file << "abc";
            }
            assets::Store assets(asset_project / "assets");
            auto with_asset = initial;
            with_asset.document_.canvas_ = {720, 1280};
            with_asset.assets_.push_back(assets.Import(source, "application/octet-stream"));
            project::Save(asset_project, with_asset);
            std::filesystem::remove(source);
            Check(project::Load(asset_project).snapshot_.assets_ == with_asset.assets_);
            Check(project::Load(asset_project).snapshot_.document_.canvas_ ==
                  with_asset.document_.canvas_);
            const auto published = std::filesystem::path(argv[2]) / "with-assets.rhythmpack";
            project::PublishSnapshot(published, with_asset, asset_project / "assets");
            Check(project::LoadPackage(published).assets_.front().bytes_ == "abc");
            const auto template_path = std::filesystem::path(argv[2]) / "asset-template";
            std::filesystem::create_directories(template_path);
            std::string asset_revision;
            {
                std::ifstream current(asset_project / "CURRENT");
                current >> asset_revision;
            }
            for (const auto& name : {"manifest.json", "graph.pb", "editor.json"})
                std::filesystem::copy_file(asset_project / "revisions" / asset_revision / name,
                                           template_path / name,
                                           std::filesystem::copy_options::overwrite_existing);
            assets::Store(template_path / "assets").CopyFrom(assets, with_asset.assets_.front());
            const auto copied_assets =
                    std::filesystem::path(argv[2]) / "template-destination-assets";
            project::AsyncStore template_worker;
            Check(template_worker.LoadTemplate(template_path, copied_assets));
            Check(!template_worker.LoadProject(asset_project));
            std::optional<project::StoreCompletion> template_result;
            const auto template_deadline =
                    std::chrono::steady_clock::now() + std::chrono::seconds(5);
            while (!template_result && std::chrono::steady_clock::now() < template_deadline) {
                template_result = template_worker.Take();
                std::this_thread::yield();
            }
            Check(template_result && template_result->template_ &&
                  template_result->error_.empty() &&
                  template_result->loaded_->snapshot_.assets_ == with_asset.assets_ &&
                  assets::Store(copied_assets).Read(with_asset.assets_.front()) == "abc");
            Reject([&] { project::PublishSnapshot(published, with_asset); });
            auto missing = with_asset;
            ++missing.assets_.front().bytes_;
            Reject([&] { project::Save(asset_project, missing); });
            Check(project::Load(asset_project).snapshot_.assets_ == with_asset.assets_);
            missing = with_asset;
            missing.assets_.push_back(missing.assets_.front());
            Reject([&] { project::Save(asset_project, missing); });
            const auto repair_source = std::filesystem::path(argv[2]) / "asset-original.bin";
            storage::WriteDurable(repair_source, "abc");
            const auto saved_asset_snapshot = project::Load(asset_project).snapshot_;
            const auto original = with_asset.assets_.front();
            const auto blob = asset_project / "assets/sha256" / original.id_.sha256_.substr(0, 2) /
                              original.id_.sha256_;
            storage::WriteDurable(blob, "bad");
            Reject([&] { project::Load(asset_project); });
            const auto repairable =
                    project::Load(asset_project, project::AssetValidation::kAllowRepair);
            Check(repairable.snapshot_ == saved_asset_snapshot &&
                  repairable.unavailable_assets_.size() == 1 &&
                  repairable.unavailable_assets_.front() == original.id_ &&
                  !repairable.warnings_.empty());
            Reject([&] { project::Save(asset_project, repairable.snapshot_); });
            Reject([&] {
                project::PublishSnapshot(published, repairable.snapshot_, asset_project / "assets");
            });
            assets.Restore(repair_source, original);
            Check(project::Load(asset_project).unavailable_assets_.empty());
            std::filesystem::remove(blob);
            Check(project::Load(asset_project, project::AssetValidation::kAllowRepair)
                          .unavailable_assets_.size() == 1);
            assets.Restore(repair_source, original);
            Check(project::Load(asset_project).snapshot_ == saved_asset_snapshot);
        }
        {
            project::AsyncStore worker;
            Check(worker.SaveProject(project_path, initial));
            Check(!worker.LoadProject(project_path));
            std::optional<project::StoreCompletion> result;
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            while (!result && std::chrono::steady_clock::now() < deadline) {
                result = worker.Take();
                std::this_thread::yield();
            }
            Check(result && result->error_.empty() && !worker.Busy());
        }
        auto next = initial;
        const auto package_path = std::filesystem::path(argv[2]) / "published.rhythmpack";
        {
            project::AsyncStore worker;
            auto captured = initial;
            Check(worker.PublishProject(package_path, captured));
            captured.title_ = "changed after submission";
            Check(!worker.PublishProject(package_path, captured));
            Check(!worker.SaveProject(project_path, captured));
            std::optional<project::StoreCompletion> result;
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            while (!result && std::chrono::steady_clock::now() < deadline) {
                result = worker.Take();
                std::this_thread::yield();
            }
            Check(result && result->error_.empty() && result->published_path_ == package_path);
            Check(project::LoadPackage(package_path).title_ == initial.title_);
            captured.document_.revision_ = 12;
            Check(worker.PublishProject(package_path, captured));
            // Destruction drains the outstanding immutable snapshot.
        }
        Check(project::LoadPackage(package_path).program_.revision_ == 12);
        next.title_ = "新的名称";
        next.document_.revision_ = 1;
        for (const auto step :
             {project::CommitStep::kGraphWritten, project::CommitStep::kEditorWritten,
              project::CommitStep::kManifestWritten, project::CommitStep::kValidated,
              project::CommitStep::kBeforeCommit}) {
            Reject([&] { project::Save(project_path, next, step); });
            Check(project::Load(project_path).snapshot_.title_ == initial.title_);
        }
        Reject([&] { project::Save(project_path, next, project::CommitStep::kCommitted); });
        Check(project::Load(project_path).snapshot_.title_ == next.title_);
        schema::GraphProject message;
        Check(message.ParseFromString(project::EncodeGraph(initial.document_)));
        message.GetReflection()->MutableUnknownFields(&message)->AddVarint(100, 42);
        message.mutable_nodes(0)->set_type_key("unknown.operator");
        auto decoded = project::DecodeGraph(message.SerializeAsString());
        Check(decoded.nodes_[0].type_ == "unknown.operator");
        Check(message.ParseFromString(project::EncodeGraph(decoded)));
        Check(message.GetReflection()->GetUnknownFields(message).field_count() == 1);
        auto duplicate = initial.document_;
        duplicate.nodes_.push_back(duplicate.nodes_.front());
        Reject([&] { project::EncodeGraph(duplicate); });
        Reject([&] { project::DecodeGraph(std::string(project::kMaximumGraphBytes + 1, 'x')); });
        {
            std::ofstream current(project_path / "CURRENT", std::ios::trunc);
            current << "../outside";
        }
        Reject([&] { project::Load(project_path); });
        project::Save(project_path, initial);
        std::string revision;
        {
            std::ifstream current(project_path / "CURRENT");
            current >> revision;
        }
        const auto manifest_path = project_path / "revisions" / revision / "manifest.json";
        std::string manifest;
        {
            std::ifstream file(manifest_path);
            manifest.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        }
        const auto write_manifest = [&](const std::string& bytes) {
            std::ofstream file(manifest_path, std::ios::trunc);
            file << bytes;
        };
        write_manifest("{\"manifest_version\":1," + manifest.substr(1));
        Reject([&] { project::Load(project_path); });
        write_manifest("{\"nested\":" + std::string(40, '[') + "0" + std::string(40, ']') + "," +
                       manifest.substr(1));
        Reject([&] { project::Load(project_path); });
        write_manifest(manifest);
        Check(project::Load(project_path).warnings_.empty());
        {
            std::ofstream editor(project_path / "revisions" / revision / "editor.json",
                                 std::ios::trunc);
            editor << "bad";
        }
        Check(project::Load(project_path).warnings_.size() == 1);
        {
            std::ofstream graph(project_path / "revisions" / revision / "graph.pb",
                                std::ios::binary | std::ios::app);
            graph << 'x';
        }
        Reject([&] { project::Load(project_path); });
        std::cout << "persistence contracts passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
