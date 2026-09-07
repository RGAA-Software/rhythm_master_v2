#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

#include "rhythm/content/user_components.h"
#include "rhythm/player/session.h"

namespace {
void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}  // namespace

int main(int argc, char* argv[]) {
    using namespace rhythm;
    try {
        Check(argc == 3, "performance_workflow template output");
        const std::filesystem::path output(argv[2]);
        const auto original = project::LoadRevision(argv[1]).snapshot_;
        Check(original.document_.nodes_.size() == 9 && original.document_.components_.size() == 2,
              "performance keeps its editable component structure");
        graph::Registry registry;
        editor::History history(original);
        auto changed = history.Current();
        changed.title_ = "星门演出 — 我的版本";
        changed.document_.nodes_[2].properties_["exposure"] = 0.35;
        changed.document_.nodes_[3].properties_["emission"] = 1.8;
        changed.positions_[3] = {70, 510};
        Check(history.Apply(changed, original.document_.revision_) && history.Undo() &&
                      history.Current().title_ == original.title_ && history.Redo(),
              "performance parameter editing supports undo and redo");
        const auto project_path = output / "performance.rhythmproj";
        project::Save(project_path, history.Current());
        const auto restored = project::Load(project_path).snapshot_;
        Check(restored.title_ == changed.title_ && restored.positions_ == changed.positions_ &&
                      restored.component_positions_ == changed.component_positions_ &&
                      project::EncodeGraph(restored.document_) ==
                              project::EncodeGraph(history.Current().document_),
              "save and reopen retain internal layout, curves and exposed controls");
        const auto package_path = output / "performance.rhythmpack";
        project::PublishSnapshot(package_path, restored);
        const auto package = project::LoadPackage(package_path);
        Check(project::EncodeProgram(package.program_) ==
                      project::EncodeProgram(std::get<graph::ExecutionPlan>(
                              graph::Compile(restored.document_, registry))),
              "published executable matches the edited project");
        Check(package.program_.instructions_.size() == 197,
              "all expected performance operators reach the final output");
        const auto unpacked =
                std::get<editor::Snapshot>(editor::UnpackComponent(restored, registry, 4, 10000));
        Check(unpacked.document_.nodes_.size() == 35 &&
                      unpacked.document_.components_ == restored.document_.components_ &&
                      unpacked.document_.nodes_[2] == restored.document_.nodes_[2],
              "unpacking the 3D core keeps the field reusable and all shared definitions intact");
        const auto unpacked_path = output / "unpacked-performance.rhythmpack";
        project::PublishSnapshot(unpacked_path, unpacked);
        Check(project::LoadPackage(unpacked_path).program_.instructions_.size() == 197,
              "unpacked performance retains the complete executable composition");

        const auto component = content::CaptureComponent(restored, 4, registry);
        for (int attempt = 0; attempt < 8; ++attempt)
            Check(content::CaptureComponent(restored, 4, registry) == component &&
                          content::CaptureComponent(component, 1, registry) == component,
                  "multi-property component identity is stable across capture and recapture");
        const auto saved = content::SaveComponent(output / "Components", component, {});
        const auto imported = content::LoadComponent(saved, output / "import-assets");
        Check(imported == component &&
                      content::SaveComponent(output / "Components", imported, {}) == saved,
              "saving a loaded component updates the same library entry");
        editor::Snapshot other;
        other.document_.id_ = "independent-performance";
        other.document_.canvas_ = {1280, 720};
        other.document_.nodes_ = {registry.MakeNode(1, "output.texture")};
        other.document_.output_ = 1;
        const auto insertion = content::InsertComponent(other, imported, registry, {0, 0}, 2);
        Check(std::holds_alternative<editor::Snapshot>(insertion),
              "3D core can be reused in an independent document");
        other = std::get<editor::Snapshot>(insertion);
        other.document_.edges_.push_back({1, 2, 1, "source"});
        const auto solo_path = output / "reused-core.rhythmpack";
        project::PublishSnapshot(solo_path, other);
        Check(project::LoadPackage(solo_path).program_.instructions_.size() == 28,
              "component publication excludes the unrelated spectrum field");

        auto renderer = render::Renderer::CreateNull();
        for (const auto& path : {package_path, solo_path, unpacked_path}) {
            player::Session session;
            session.Open(path);
            runtime::ExternalInputs inputs;
            inputs.audio_.emplace();
            inputs.audio_->valid_ = true;
            inputs.audio_->generation_ = 1;
            inputs.audio_->sample_rate_ = 48000;
            runtime::PlaybackSample music;
            music.generation_ = 1;
            music.duration_ = 30;
            for (int frame = 0; frame < 150; ++frame) {
                if (frame == 50 || frame == 110) music.paused_ = true;
                if (frame == 65 || frame == 120) music.paused_ = false;
                if (frame == 60 || frame == 100) {
                    music.seconds_ = frame == 60 ? 1.5 : 8;
                    ++music.generation_;
                } else if (!music.paused_) {
                    music.seconds_ += 1.0 / 30;
                }
                inputs.audio_->center_seconds_ = music.seconds_;
                renderer.BeginFrame();
                const auto image =
                        session.Tick(frame / 30.0, false, {640, 360}, renderer, inputs, music);
                Check(renderer.IsValid(image.final_) && session.Seconds() == music.seconds_ &&
                              session.Paused() == music.paused_,
                      "published performance follows shared music pause and seek");
                renderer.EndFrame();
                if (frame == 80) session.ReleaseGraphics();
            }
        }
        std::cout << "performance workflow: editable controls, undo, save/reopen, publication, "
                     "component reuse, music transport and device recreation passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
