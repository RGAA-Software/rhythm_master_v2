#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

#include "rhythm/assets/store.h"
#include "rhythm/content/component_library.h"
#include "rhythm/content/user_components.h"
#include "rhythm/graph/components.h"

namespace {
void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
}  // namespace
int main(int argc, char* argv[]) {
    using namespace rhythm;
    try {
        Check(argc == 3, "user_components <test directory> <png>");
        const std::filesystem::path root(argv[1]);
        graph::Registry registry;
        graph::ComponentDefinition inner;
        inner.type_ = "component.user.1";
        inner.title_ = "Inner";
        inner.nodes_ = {registry.MakeNode(11, "scalar.constant")};
        inner.output_ = 11;
        inner.parameters_ = {{"value", 11, "value", "Controls"}};
        graph::ComponentDefinition outer;
        outer.type_ = "component.user.2";
        outer.title_ = "Reusable control";
        outer.nodes_ = {registry.MakeNode(21, inner.type_, std::array{inner})};
        outer.output_ = 21;
        outer.parameters_ = {{"level", 21, "value", "Music"}};
        editor::Snapshot source;
        source.document_.id_ = "authoring";
        source.document_.components_ = {inner, outer};
        source.document_.nodes_ = {registry.MakeNode(40, outer.type_, source.document_.components_),
                                   registry.MakeNode(50, "texture.gradient")};
        source.document_.nodes_[0].properties_["level"] = 0.7;
        source.document_.output_ = 50;
        source.component_positions_[inner.type_][11] = {31, 42};
        const auto captured = content::CaptureComponent(source, 40, registry);
        Check(captured.document_.nodes_.size() == 1 && captured.document_.components_.size() == 2 &&
                      captured.document_.nodes_[0].properties_.at("level") == graph::Property{0.7},
              "capture keeps nested closure and instance controls only");
        Check(content::CaptureComponent(captured, 1, registry) == captured,
              "capturing a library instance again is stable");
        const auto saved =
                content::SaveComponent(root / "library", captured, root / "source/assets");
        const auto restored = content::LoadComponent(saved, root / "destination/assets");
        Check(restored == captured, "project codec retains grouped controls and nested layout");
        auto destination = source;
        destination.document_.id_ = "another-project";
        destination.document_.components_[0].nodes_[0].properties_["value"] = 0.1;
        const auto inserted =
                content::InsertComponent(destination, restored, registry, {300, 200}, 100);
        Check(std::holds_alternative<editor::Snapshot>(inserted),
              "local type collision is isolated");
        const auto& next = std::get<editor::Snapshot>(inserted);
        Check(next.document_.components_.size() == 4 && next.document_.output_ == 50 &&
                      next.positions_.at(100) == editor::Position{300, 200},
              "insertion preserves destination identity and output");
        const auto expanded =
                std::get<graph::Document>(graph::ExpandComponents(next.document_, registry));
        const auto output = std::find_if(expanded.nodes_.begin(), expanded.nodes_.end(),
                                         [](const auto& node) { return node.id_ == 100; });
        Check(output != expanded.nodes_.end() && graph::Scalar(*output, "value", 0) == 0.7,
              "nested exposed parameter reaches executable node");
        const auto again = content::InsertComponent(next, restored, registry, {}, 101);
        Check(std::get<editor::Snapshot>(again).document_.components_.size() == 4,
              "unchanged library definitions are reused");
        editor::History history(destination);
        Check(history.Apply(next, destination.document_.revision_) && history.Undo() &&
                      history.Current().document_.components_ == destination.document_.components_,
              "one insertion is one undoable edit");

        assets::Store originals(root / "source/assets");
        const auto image = originals.Import(argv[2], "image/png");
        graph::ComponentDefinition picture;
        picture.type_ = "component.user.picture";
        picture.title_ = "Picture";
        picture.nodes_ = {registry.MakeNode(1, "texture.image")};
        picture.nodes_[0].properties_["asset"] = image.id_;
        picture.output_ = 1;
        source.document_.components_.push_back(picture);
        source.document_.nodes_.push_back(
                registry.MakeNode(60, picture.type_, source.document_.components_));
        source.assets_ = {image};
        const auto image_component = content::CaptureComponent(source, 60, registry);
        Check(image_component.assets_.size() == 1 &&
                      image_component.document_.components_.size() == 1,
              "asset component capture has no unrelated definitions");
        const auto image_saved =
                content::SaveComponent(root / "library", image_component, root / "source/assets");
        const auto loaded_image = content::LoadComponent(image_saved, root / "destination/assets");
        Check(assets::Store(root / "destination/assets").Verify(image) &&
                      loaded_image == image_component,
              "immutable assets travel with the saved component");
        auto invalid = captured;
        invalid.document_.components_.front().nodes_.front().type_ = "missing.operator";
        Check(std::holds_alternative<graph::Diagnostic>(
                      content::InsertComponent(destination, invalid, registry, {}, 102)),
              "invalid library cannot mutate the project");
        content::ComponentLibrary library(root / "async-library");
        const auto completion = [&] {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            while (std::chrono::steady_clock::now() < deadline) {
                if (auto result = library.Take()) {
                    Check(result->error_.empty(), "library worker failed");
                    return std::move(*result);
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
            throw std::runtime_error("library worker timed out");
        };
        completion();
        Check(library.Save(source, 40, root / "source/assets") && !library.Refresh(),
              "single bounded operation rejects overlap");
        const auto stored = completion();
        Check(stored.entries_ && !stored.entries_->empty() && !stored.saved_.empty(),
              "saved component appears in the managed library");
        Check(library.Load(stored.saved_, root / "destination/assets", "another-project", 17),
              "library insertion load queued");
        const auto ready = completion();
        Check(ready.component_ && ready.expected_document_ == "another-project" &&
                      ready.expected_revision_ == 17,
              "worker completion carries the optimistic edit boundary");
        std::cout << "user components: nested capture, collision isolation, persistence/assets, "
                     "insertion and undo passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
