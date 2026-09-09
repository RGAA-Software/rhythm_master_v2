#include <chrono>
#include <iostream>
#include <stdexcept>

#include "rhythm/assets/store.h"
#include "rhythm/editor/commands.h"
#include "rhythm/player/session.h"
#include "rhythm/project/package.h"
#include "rhythm/project/store.h"

int main(int argc, char* argv[]) {
    using namespace rhythm;
    try {
        if (argc != 2) throw std::invalid_argument("template.arguments");
        const auto templates = project::ScanTemplates(argv[1]);
        bool portrait = false;
        bool square = false;
        std::size_t defaults = 0;
        editor::Snapshot current;
        current.document_.id_ = "template-switch-regression";
        graph::Registry registry;
        current.document_.nodes_ = {registry.MakeNode(10000, "texture.gradient"),
                                    registry.MakeNode(10001, "output.texture")};
        current.document_.output_ = 10001;
        current.document_.edges_ = {{1, 10000, 10001, "source"}};
        editor::History history(current);
        const auto workspace =
                std::filesystem::path(argv[1]).parent_path() / "template-switch-tests" /
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        for (const auto& entry : templates) {
            defaults += entry.default_ ? 1 : 0;
            const auto source = project::LoadRevision(entry.directory_);
            if (!source.warnings_.empty()) throw std::runtime_error("template.layout");
            const auto& document = source.snapshot_.document_;
            std::vector<graph::NodeId> ids;
            for (std::size_t index = 0; index < document.nodes_.size(); ++index)
                ids.push_back(history.ReserveNodeId());
            const auto applied =
                    editor::InstantiateTemplate(history.Current(), source.snapshot_, ids);
            if (!std::holds_alternative<editor::Snapshot>(applied))
                throw std::runtime_error(entry.id_ + ": template application rejected");
            const auto& remapped = std::get<editor::Snapshot>(applied);
            const auto compiled = graph::Compile(remapped.document_, registry);
            if (!std::holds_alternative<graph::ExecutionPlan>(compiled))
                throw std::runtime_error(entry.id_ + ": applied template cannot compile");
            const auto& plan = std::get<graph::ExecutionPlan>(compiled);
            if (remapped.document_.control_cues_ != document.control_cues_ ||
                remapped.document_.beat_grid_ != document.beat_grid_ ||
                remapped.assets_ != source.snapshot_.assets_ ||
                remapped.soundtrack_ != source.snapshot_.soundtrack_)
                throw std::runtime_error(entry.id_ + ": application changed cues or media");
            const auto before = history.Current();
            if (!history.Apply(remapped, before.document_.revision_) || !history.Undo() ||
                history.Current().document_.nodes_ != before.document_.nodes_ || !history.Redo())
                throw std::runtime_error(entry.id_ + ": application undo/redo failed");
            portrait |= document.canvas_.height_ > document.canvas_.width_;
            square |= document.canvas_.height_ == document.canvas_.width_;
            std::vector<project::PackagedAsset> assets;
            if (!source.snapshot_.assets_.empty()) {
                const assets::Store store(entry.directory_ / "assets");
                for (const auto& record : source.snapshot_.assets_)
                    assets.push_back(
                            {record, store.Read(record, project::kMaximumPackageAssetBytes)});
            }
            const auto bytes = project::EncodePackage(document, source.snapshot_.title_, assets,
                                                      source.snapshot_.soundtrack_);
            const auto packaged = project::DecodePackage(bytes);
            const auto project_path = workspace / entry.directory_.filename();
            const auto prepared =
                    project::PrepareTemplate(entry.directory_, project_path / "assets");
            if (prepared.snapshot_.assets_ != remapped.assets_)
                throw std::runtime_error(entry.id_ + ": prepared assets differ");
            project::Save(project_path, history.Current());
            const auto reopened = project::Load(project_path).snapshot_;
            const auto reapplied = project::DecodePackage(project::EncodePackage(
                    reopened.document_, reopened.title_, assets, reopened.soundtrack_));
            if (reapplied.program_.instructions_.size() != packaged.program_.instructions_.size() ||
                reapplied.program_.controls_ != plan.controls_ ||
                reapplied.program_.beat_grid_ != document.beat_grid_)
                throw std::runtime_error(entry.id_ + ": applied/save/reopen publication differs");
            if (packaged.soundtrack_ != source.snapshot_.soundtrack_)
                throw std::runtime_error("template.soundtrack");
            // Valid UTF-8 and available glyphs do not detect GBK-decoded mojibake.
            // Check the actual catalog label and published title, not a sample string.
            if (entry.id_ == "official.templates.prismatic_lotus") {
                const std::string expected = "棱镜星莲";
                if (entry.titles_.at("zh-CN") != expected ||
                    source.snapshot_.title_ != expected + " / Prismatic lotus" ||
                    packaged.title_ != source.snapshot_.title_)
                    throw std::runtime_error("template.localized_title");
            }
            if (packaged.program_.canvas_ != document.canvas_)
                throw std::runtime_error("template.canvas");
            auto renderer = render::Renderer::CreateNull();
            player::Session session;
            session.Load(project::EncodePackage(reopened.document_, reopened.title_, assets,
                                                reopened.soundtrack_));
            for (int frame = 0; frame < 60; ++frame) {
                renderer.BeginFrame();
                const auto output = session.Tick(frame / 60.0, false, session.Canvas(), renderer);
                if (!renderer.IsValid(output.final_)) throw std::runtime_error("template.output");
                renderer.EndFrame();
            }
            std::cout << entry.id_
                      << ": apply/remap/undo/redo/save/reopen/publish, 60 runtime frames, "
                      << document.canvas_.width_ << "x" << document.canvas_.height_ << '\n';
        }
        if (templates.size() < 3 || defaults != 1 || !portrait || !square)
            throw std::runtime_error("template.catalog_coverage");
        std::cout << "template contracts passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
