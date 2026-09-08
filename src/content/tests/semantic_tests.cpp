#include <iostream>
#include <stdexcept>

#include "rhythm/content/semantic.h"
#include "rhythm/project/package.h"
#include "rhythm/runtime/runtime.h"

namespace {
void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}  // namespace
int main(int argc, char* argv[]) {
    using namespace rhythm;
    try {
        Check(argc == 2 || argc == 3, "catalog path and optional variant output directory");
        graph::Registry registry;
        const auto catalog = content::LoadSemantics(argv[1], registry);
        Check(catalog.size() == 25, "semantic catalog coverage");
        editor::Snapshot original;
        original.document_.id_ = "semantic.test";
        original.document_.nodes_ = {registry.MakeNode(1, "texture.gradient"),
                                     registry.MakeNode(2, "output.texture")};
        original.document_.edges_ = {{1, 1, 2, "source"}};
        original.document_.output_ = 2;
        for (const auto& semantic : catalog) {
            editor::History history(original);
            const auto id = history.ReserveNodeId();
            auto added = content::AddSemantic(original, semantic, registry, {300, 100}, id);
            Check(std::holds_alternative<editor::Snapshot>(added), "semantic insertion");
            const auto next = std::get<editor::Snapshot>(added);
            Check(next.document_.nodes_.size() == 3 &&
                          next.document_.edges_ == original.document_.edges_ &&
                          next.document_.id_ == original.document_.id_,
                  "insertion preserves the existing graph");
            Check(history.Apply(next, 0) && history.Undo() &&
                          history.Current().document_.nodes_ == original.document_.nodes_ &&
                          history.Current().document_.components_.empty() && history.Redo(),
                  "whole semantic insertion is undoable");
            auto connected =
                    std::get<editor::Snapshot>(editor::Connect(next, registry, id, 2, "source"));
            const auto descriptor =
                    registry.Find(semantic.root_.type_, semantic.content_.document_.components_);
            for (const auto& input : descriptor->inputs_) {
                if (!input.required_) continue;
                Check(input.type_ == graph::ValueType::kTexture,
                      "input-processing component accepts the user's texture");
                connected = std::get<editor::Snapshot>(
                        editor::Connect(connected, registry, 1, id, input.key_));
            }
            Check(semantic.presets_.size() == 2, "default and curated variant available");
            auto& root = connected.document_.nodes_.back();
            const auto initial = root;
            const auto& definitions = connected.document_.components_;
            root = content::ApplyPreset(root, semantic.presets_[1], registry, definitions);
            Check(root != initial, "variant changes authored values");
            root = content::ApplyPreset(root, semantic.presets_[0], registry, definitions);
            Check(root == initial, "default restores the entire exposed configuration");
            const auto roundtrip = project::DecodeGraph(project::EncodeGraph(connected.document_));
            const auto package =
                    project::DecodePackage(project::EncodePackage(roundtrip, "semantic"));
            auto renderer = render::Renderer::CreateNull();
            runtime::Runtime runtime;
            for (int frame = 0; frame < 60; ++frame) {
                renderer.BeginFrame();
                const auto result =
                        runtime.Evaluate(package.program_, {frame / 60.0, 0, {640, 360}}, renderer);
                Check(renderer.IsValid(result.final_), "semantic runtime output");
                renderer.EndFrame();
            }
            root = content::ApplyPreset(root, semantic.presets_[1], registry, definitions);
            const auto variant_package =
                    project::DecodePackage(project::EncodePackage(connected.document_, "variant"));
            if (argc == 3) {
                auto name = semantic.metadata_.directory_.filename();
                name += ".rhythmpack";
                project::PublishSnapshot(std::filesystem::path(argv[2]) / name, connected);
            }
            runtime.Reset();
            for (int frame = 0; frame < 60; ++frame) {
                renderer.BeginFrame();
                const auto result = runtime.Evaluate(variant_package.program_,
                                                     {frame / 60.0, 0, {640, 360}}, renderer);
                Check(renderer.IsValid(result.final_), "variant runtime output");
                renderer.EndFrame();
            }
            auto changed = next;
            changed.document_.components_.front().title_ += " edited";
            const auto conflict = content::AddSemantic(changed, semantic, registry, {}, id + 1);
            Check(std::holds_alternative<graph::Diagnostic>(conflict) &&
                          std::get<graph::Diagnostic>(conflict).code_ ==
                                  "content.semantic_conflict",
                  "catalog never overwrites a locally modified component");
            auto again = content::AddSemantic(next, semantic, registry, {}, id + 1);
            Check(std::get<editor::Snapshot>(again).document_.components_.size() ==
                          next.document_.components_.size(),
                  "identical definitions are shared by inserted instances");
            std::cout << semantic.metadata_.id_
                      << ": insertion, history, package and 60 frames passed\n";
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
