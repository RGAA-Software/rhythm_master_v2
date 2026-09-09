#include "rhythm/studio/studio.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <array>
#include <cstring>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <optional>

#include "asset_panel.h"
#include "beat_performance.h"
#include "canvas_settings.h"
#include "component_library_panel.h"
#include "component_panel.h"
#include "component_workbench.h"
#include "graph_canvas.h"
#include "input_preview.h"
#include "node_palette.h"
#include "performance_panel.h"
#include "preview_routing.h"
#include "property_inspector.h"
#include "rhythm/audio_ui/audio_panel.h"
#include "rhythm/content/presets.h"
#include "rhythm/cyber/theme.h"
#include "rhythm/editor/commands.h"
#include "rhythm/editor/compiler_worker.h"
#include "rhythm/prepared_assets/loader.h"
#include "rhythm/project/async_store.h"
#include "rhythm/project/store.h"
#include "rhythm/render/layout.h"
#include "rhythm/runtime/runtime.h"
#include "rhythm/runtime/viewers.h"
#include "rhythm/video_sources/streams.h"
#include "semantic_palette.h"
#include "shader_panel.h"
#include "template_browser.h"
#include "timeline_panel.h"
#ifdef RHYTHM_HAS_LOCAL_MEDIA
#include "export_panel.h"
#include "soundtrack_panel.h"
#endif

namespace rhythm::studio {
class Studio::Impl final {
   public:
    Impl(const std::filesystem::path& resources, const std::filesystem::path& project)
        : project_(project) {
        shader_panel_.SetTools({resources / "shader_tools/shaderc.exe",
                                resources / "shader_tools/include",
                                resources / "shader_tools/varying.def.sc"});
#ifdef RHYTHM_HAS_LOCAL_MEDIA
        audio_panel_.SetDemoFile(resources / "content/audio/resonance_demo.wav");
#endif
        for (const auto& locale : {"zh-CN", "en-US"}) {
            std::ifstream file(resources / "locales" / locale / "studio.json");
            catalogs_[locale] =
                    nlohmann::json::parse(file).get<std::map<std::string, std::string>>();
        }
        templates_ = project::ScanTemplates(resources / "content" / "templates");
        if (templates_.empty()) throw std::runtime_error("content.empty_catalog");
        const auto preferred = std::find_if(templates_.begin(), templates_.end(),
                                            [](const auto& item) { return item.default_; });
        const auto template_path =
                (preferred == templates_.end() ? templates_.front() : *preferred).directory_;
        const auto loaded =
                std::filesystem::exists(project_ / "CURRENT")
                        ? project::Load(project_, project::AssetValidation::kAllowRepair)
                        : project::PrepareTemplate(template_path, project_ / "assets");
        history_.emplace(loaded.snapshot_);
        unavailable_assets_ = loaded.unavailable_assets_;
#ifdef RHYTHM_HAS_LOCAL_MEDIA
        soundtrack_panel_.Sync(history_->Current(), project_ / "assets", audio_panel_,
                               unavailable_assets_);
#endif
        presets_ = content::LoadPresets(resources / "content/presets/catalog.json", registry_);
        semantics_ = content::LoadSemantics(resources / "content/semantic", registry_);
        for (const auto& entry : semantics_) {
            presets_.insert(presets_.end(), entry.presets_.begin(), entry.presets_.end());
            for (const auto& [locale, title] : entry.metadata_.titles_)
                catalogs_[locale][entry.root_.type_] = title;
        }
        diagnostics_ = loaded.warnings_;
        SyncTitle();
        QueueCompile();
        cyber::ApplyTheme();
    }
    std::string Text(const std::string& key) const {
        const auto& catalog = catalogs_.at(locale_);
        const auto found = catalog.find(key);
        return found == catalog.end() ? key : found->second;
    }
    std::string Label(const std::string& key) const { return Text(key) + "###" + key; }
    void MoveHistory(bool redo) {
        const auto preview = inspector_.Preview().has_value() || timeline_.Preview().has_value();
        auto before = history_->Current().document_;
        const auto before_assets = history_->Current().assets_;
        if (!(redo ? history_->Redo() : history_->Undo())) return;
        auto after = history_->Current().document_;
        before.revision_ = 0;
        after.revision_ = 0;
        SyncTitle();
        canvas_.RestoreLayout();
        if (before != after || before_assets != history_->Current().assets_ || preview)
            QueueCompile();
    }
    void SyncTitle() {
        beat_performance_.Reset();
        title_.fill(0);
        const auto& title = history_->Current().title_;
        std::memcpy(title_.data(), title.data(), std::min(title.size(), title_.size() - 1));
        inspector_.Reset();
        timeline_.ResetEdit();
    }
    void QueueCompile() {
        beat_performance_.Cancel();
        asset_loader_.Cancel();
        const auto& document = component_workbench_.PreviewDocument()
                                       ? *component_workbench_.PreviewDocument()
                               : inspector_.Preview() ? inspector_.Preview()->document_
                               : timeline_.Preview()  ? timeline_.Preview()->document_
                                                      : history_->Current().document_;
        auto request = preview_routing_.Prepare(
                viewer_nodes_,
                show_viewers_ ? component_workbench_.PreviewViewers() : editor::ScopedViewers{},
                document.id_);
        generation_ =
                compiler_.Submit(document, std::move(request.roots_), std::move(request.scoped_));
    }
    void Apply(editor::Snapshot next) {
        const auto assets_changed = next.assets_ != history_->Current().assets_;
        auto before = history_->Current().document_;
        auto after = next.document_;
        before.revision_ = 0;
        after.revision_ = 0;
        const auto expected_revision = next.document_.revision_;
        if (history_->Apply(std::move(next), expected_revision) &&
            (before != after || assets_changed))
            QueueCompile();
    }
    void Toolbar(platform::Host& host, render::Renderer& renderer, double seconds) {
        if (show_viewers_) preview_routing_.DrawNavigation(catalogs_.at(locale_));
        bool busy = store_.Busy() || assets_.Busy();
#ifdef RHYTHM_HAS_LOCAL_MEDIA
        busy |= soundtrack_panel_.Busy();
#endif
        ImGui::BeginDisabled(busy);
        if (ImGui::Button(Label("save").c_str())) {
            CommitEdits();
            store_.SaveProject(project_, history_->Current());
        }
        ImGui::SameLine();
        if (ImGui::Button(Label("publish").c_str())) {
            CommitEdits();
            auto name = project_.filename();
            name.replace_extension(".rhythmpack");
            store_.PublishProject(project_.parent_path().parent_path() / "Published" / name,
                                  history_->Current(), project_ / "assets");
        }
#ifdef RHYTHM_HAS_LOCAL_MEDIA
        ImGui::SameLine();
        if (ImGui::Button(Label("export.open").c_str())) {
            CommitEdits();
            auto name = project_.filename();
            name.replace_extension(".mp4");
            const auto music = audio_panel_.Frame().playback_;
            export_panel_.Open(project_.parent_path().parent_path() / "Exports" / name,
                               audio_panel_.SelectedFile(),
                               music && music->duration_ ? *music->duration_ : 10,
                               audio_panel_.Volume(), history_->Current().document_.canvas_,
                               history_->Current().soundtrack_ &&
                                       !history_->Current().soundtrack_->clips_.empty());
        }
#endif
        ImGui::SameLine();
        if (ImGui::Button(Label("reopen").c_str())) {
            load_revision_ = history_->Current().document_.revision_;
            store_.LoadProject(project_, project::AssetValidation::kAllowRepair);
        }
        ImGui::SameLine();
        if (const auto selected =
                    template_browser_.Draw(templates_, locale_, catalogs_.at(locale_), host,
                                           renderer, seconds, preview_inputs_)) {
            CommitEdits();
            load_revision_ = history_->Current().document_.revision_;
            store_.LoadTemplate(templates_[*selected].directory_, project_ / "assets");
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        const auto asset_edit =
                assets_.Draw(project_ / "assets", history_->Current(), catalogs_.at(locale_));
        if (asset_edit.added_ || asset_edit.removed_) {
            CommitEdits();
            if (asset_edit.added_) std::erase(unavailable_assets_, asset_edit.added_->id_);
            editor::EditResult result = history_->Current();
            if (asset_edit.replaced_ && asset_edit.added_)
                result = editor::ReplaceAsset(history_->Current(), *asset_edit.replaced_,
                                              *asset_edit.added_);
            else if (asset_edit.removed_)
                result = editor::RemoveUnusedAsset(history_->Current(), *asset_edit.removed_);
            else if (asset_edit.added_) {
                auto next = history_->Current();
                if (std::none_of(next.assets_.begin(), next.assets_.end(), [&](const auto& asset) {
                        return asset.id_ == asset_edit.added_->id_;
                    }))
                    next.assets_.push_back(*asset_edit.added_);
                result = std::move(next);
            }
            if (std::holds_alternative<editor::Snapshot>(result)) {
                Apply(std::get<editor::Snapshot>(std::move(result)));
                if (asset_edit.replaced_) assets_.Report("asset.replaced", asset_edit.added_->id_);
            } else {
                const auto code = std::get<graph::Diagnostic>(result).code_;
                status_ = Text(code);
                assets_.Report(code, asset_edit.replaced_.value_or(assets::AssetId{}));
            }
        }
        if (asset_edit.checked_) {
            for (const auto& check : *asset_edit.checked_) {
                std::erase(unavailable_assets_, check.asset_.id_);
                if (check.health_ != assets::AssetHealth::kValid)
                    unavailable_assets_.push_back(check.asset_.id_);
            }
        }
        if (asset_edit.restored_) {
            std::erase(unavailable_assets_, *asset_edit.restored_);
            force_asset_reload_ = true;
            QueueCompile();
        }
        ImGui::SameLine();
        if (ImGui::Button(Label("undo").c_str())) MoveHistory(false);
        ImGui::SameLine();
        if (ImGui::Button(Label("redo").c_str())) MoveHistory(true);
        ImGui::SameLine();
        if (ImGui::Button(locale_ == "zh-CN" ? "English###locale" : "简体中文###locale"))
            locale_ = locale_ == "zh-CN" ? "en-US" : "zh-CN";
        ImGui::Checkbox(Label("viewers").c_str(), &show_viewers_);
        ImGui::SameLine();
        ImGui::Checkbox(Label("timeline").c_str(), &show_timeline_);
        ImGui::SameLine();
        if (ImGui::Button(Label("reset").c_str())) timeline_.Restart();
        ImGui::SameLine();
        if (const auto selected =
                    semantic_palette_.Draw(semantics_, locale_, catalogs_.at(locale_), host,
                                           renderer, seconds, preview_inputs_)) {
            CommitEdits();
            const auto& semantic = semantics_.at(*selected);
            if (!semantic.content_.assets_.empty()) {
                const bool accepted = component_library_.StartOfficial(
                        semantic, history_->Current(), project_ / "assets",
                        canvas_.InsertionPoint());
                status_ = Text(accepted ? "component.library_preparing" : "component.library_busy");
            } else {
                const auto id = history_->ReserveNodeId();
                auto edit = content::AddSemantic(history_->Current(), semantic, registry_,
                                                 canvas_.InsertionPoint(), id);
                if (std::holds_alternative<editor::Snapshot>(edit)) {
                    Apply(std::get<editor::Snapshot>(std::move(edit)));
                    canvas_.RestoreLayout();
                    canvas_.Select(id);
                } else
                    status_ = Text(std::get<graph::Diagnostic>(edit).code_);
            }
        }
        ImGui::SameLine();
        if (const auto type = node_palette_.Draw(registry_.Operators(), catalogs_.at(locale_),
                                                 "node.palette")) {
            CommitEdits();
            const auto id = history_->ReserveNodeId();
            auto result = editor::AddNode(history_->Current(), registry_, *type,
                                          canvas_.InsertionPoint(), id);
            if (std::holds_alternative<editor::Snapshot>(result)) {
                Apply(std::get<editor::Snapshot>(std::move(result)));
                canvas_.RestoreLayout();
                canvas_.Select(id);
            } else {
                status_ = Text(std::get<graph::Diagnostic>(result).code_);
            }
        }
        ImGui::InputText((Text("title") + "###title").c_str(), title_.data(), title_.size());
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            auto next = history_->Current();
            next.title_ = title_.data();
            Apply(std::move(next));
        }
        if (!status_.empty()) ImGui::TextUnformatted(status_.c_str());
        const auto& current_assets = history_->Current().assets_;
        if (std::any_of(current_assets.begin(), current_assets.end(), [&](const auto& record) {
                return std::find(unavailable_assets_.begin(), unavailable_assets_.end(),
                                 record.id_) != unavailable_assets_.end();
            }))
            ImGui::TextWrapped("%s", Text("project.assets_need_repair").c_str());
        if (const auto canvas = DrawCanvasSettings(history_->Current().document_.canvas_,
                                                   catalogs_.at(locale_))) {
            CommitEdits();
            auto next = history_->Current();
            next.document_.canvas_ = *canvas;
            Apply(std::move(next));
        }
    }
    void Inspector() {
        ImGui::BeginDisabled(inspector_.Preview().has_value());
        const auto beat_edit = beat_performance_.Draw(
                history_->Current(), evaluated_seconds_.value_or(0), catalogs_.at(locale_));
        ImGui::EndDisabled();
        if (beat_edit) {
            CommitEdits();
            auto next = history_->Current();
            next.document_.beat_grid_ = beat_edit->document_.beat_grid_;
            Apply(std::move(next));
        }
        if (const auto request = component_library_.Draw(
                    history_->Current().document_, canvas_.Selections(), catalogs_.at(locale_))) {
            CommitEdits();
            component_library_.Start(*request, history_->Current(), project_ / "assets",
                                     canvas_.InsertionPoint());
        }
        if (const auto action = component_panel_.Draw(
                    history_->Current().document_, canvas_.Selections(), catalogs_.at(locale_))) {
            CommitEdits();
            if (action->kind_ == ComponentActionKind::kEdit) {
                component_workbench_.Open(history_->Current(), action->value_, canvas_.Selection());
            } else {
                const auto fresh_id = history_->ReserveNodeId();
                auto edit = ExecuteComponentAction(*action, history_->Current(), registry_,
                                                   canvas_.Selections(), fresh_id,
                                                   canvas_.InsertionPoint());
                if (std::holds_alternative<editor::Snapshot>(edit)) {
                    Apply(std::get<editor::Snapshot>(std::move(edit)));
                    canvas_.RestoreLayout();
                    if (action->kind_ == ComponentActionKind::kCreate ||
                        action->kind_ == ComponentActionKind::kAdd)
                        canvas_.Select(fresh_id);
                } else
                    status_ = Text(std::get<graph::Diagnostic>(edit).code_);
            }
        }
        shader_panel_.Draw(history_->Current(), canvas_.Selection(), project_ / "assets",
                           catalogs_.at(locale_));
        auto result = inspector_.Draw(history_->Current(), canvas_.Selection(), registry_, presets_,
                                      catalogs_.at(locale_), locale_, *prepared_resources_->models_,
                                      evaluated_seconds_.value_or(0),
                                      history_->Current().document_.beat_grid_.has_value());
        if (result.follow_cues_) beat_performance_.Cancel(player::PerformanceActionReason::kUser);
        if (result.recall_) {
            if (plan_ && plan_generation_ == generation_ && diagnostics_.empty())
                beat_performance_.RequestSnapshot(*result.recall_);
            else
                status_ = Text("beat.wait_plan");
        }
        if (result.diagnostic_) status_ = Text(result.diagnostic_->code_);
        if (result.committed_)
            Apply(std::move(*result.committed_));
        else if (result.preview_changed_)
            QueueCompile();
    }
    void Frame(platform::Host& host, render::Renderer& renderer, double seconds) {
        component_library_.Initialize(host.DataDirectory() / "Components");
        if (!inspector_.Preview() && !timeline_.Preview())
            if (auto shader = shader_panel_.Take(history_->Current())) Apply(std::move(*shader));
#ifdef RHYTHM_HAS_LOCAL_MEDIA
        if (auto edit = soundtrack_panel_.Take(
                    history_->Current(),
                    inspector_.Preview().has_value() || timeline_.Preview().has_value() ||
                            std::string(title_.data()) != history_->Current().title_,
                    audio_panel_))
            Apply(std::move(*edit));
#endif
        if (const auto completed = store_.Take()) {
            if (!completed->error_.empty()) {
                std::cerr << completed->error_ << '\n';
                status_ = Text(catalogs_.at(locale_).contains(completed->error_)
                                       ? completed->error_
                                       : "operation_failed");
            } else if (completed->loaded_) {
                if (history_->Current().document_.revision_ != load_revision_ ||
                    inspector_.Preview() || timeline_.Preview() ||
                    std::string(title_.data()) != history_->Current().title_)
                    status_ = Text("load_conflict");
                else if (completed->template_) {
                    std::vector<graph::NodeId> ids;
                    for (std::size_t index = 0;
                         index < completed->loaded_->snapshot_.document_.nodes_.size(); ++index)
                        ids.push_back(history_->ReserveNodeId());
                    const auto result = editor::InstantiateTemplate(
                            history_->Current(), completed->loaded_->snapshot_, ids);
                    if (std::holds_alternative<editor::Snapshot>(result)) {
                        viewer_nodes_.clear();
                        Apply(std::get<editor::Snapshot>(result));
                        SyncTitle();
                        canvas_.RestoreLayout();
                        ++reset_;
                        timeline_.Restart();
                        status_ = Text("templates.applied");
                    } else
                        status_ = Text(std::get<graph::Diagnostic>(result).code_);
                } else {
                    history_.emplace(completed->loaded_->snapshot_);
                    unavailable_assets_ = completed->loaded_->unavailable_assets_;
                    diagnostics_ = completed->loaded_->warnings_;
                    SyncTitle();
                    canvas_.RestoreLayout();
                    QueueCompile();
                    ++reset_;
                    timeline_.Restart();
                    status_ = Text("reopened");
                }
            } else if (!completed->published_path_.empty()) {
                const auto path = completed->published_path_.u8string();
                status_ = Text("published") + " " + std::string(path.begin(), path.end());
            } else
                status_ = Text("saved") + " " + std::to_string(completed->saved_revision_);
        }
        if (const auto insertion = component_library_.Take()) {
            const auto& result = insertion->result_;
            if (!result.error_.empty()) {
                status_ = Text(result.error_);
            } else if (history_->Current().document_.id_ != result.expected_document_ ||
                       history_->Current().document_.revision_ != result.expected_revision_ ||
                       inspector_.Preview() || timeline_.Preview() ||
                       std::string(title_.data()) != history_->Current().title_) {
                status_ = Text("load_conflict");
            } else {
                const auto id = history_->ReserveNodeId();
                auto edit = content::InsertComponent(history_->Current(), *result.component_,
                                                     registry_, insertion->position_, id);
                if (std::holds_alternative<editor::Snapshot>(edit)) {
                    Apply(std::get<editor::Snapshot>(std::move(edit)));
                    canvas_.RestoreLayout();
                    canvas_.Select(id);
                } else {
                    status_ = Text(std::get<graph::Diagnostic>(edit).code_);
                }
            }
        }
        if (const auto completed = compiler_.Take();
            completed && completed->generation_ == generation_) {
            if (std::holds_alternative<graph::ExecutionPlan>(completed->result_)) {
                preview_routing_.Stage(*completed);
                auto next = std::get<graph::ExecutionPlan>(completed->result_);
                const auto& records = history_->Current().assets_;
                const bool listed = std::all_of(
                        next.instructions_.begin(), next.instructions_.end(),
                        [&](const auto& instruction) {
                            if (instruction.operation_ != graph::Operation::kGeometryGlb &&
                                instruction.operation_ != graph::Operation::kTextureImage &&
                                instruction.operation_ != graph::Operation::kTextureShader &&
                                instruction.operation_ != graph::Operation::kTextureVideo)
                                return true;
                            const auto& id = std::get<assets::AssetId>(
                                    instruction.node_.properties_.at("asset"));
                            return std::any_of(
                                    records.begin(), records.end(),
                                    [&](const auto& record) { return record.id_ == id; });
                        });
                if (listed && !force_asset_reload_ &&
                    prepared_assets::Covers(next, *prepared_resources_)) {
                    plan_ = std::move(next);
                    plan_generation_ = generation_;
                    preview_routing_.Commit();
                    diagnostics_.clear();
                } else {
                    try {
                        asset_loader_.Submit(
                                {std::move(next), records, project_ / "assets", generation_});
                        status_ = Text("asset.preparing");
                    } catch (const std::exception& error) {
                        std::cerr << error.what() << '\n';
                        diagnostics_ = {{"asset.prepare_failed"}};
                        status_ = Text("asset.prepare_failed");
                    }
                }
            } else
                diagnostics_ = std::get<std::vector<graph::Diagnostic>>(completed->result_);
        }
        if (auto completed = asset_loader_.Take();
            completed && completed->generation_ == generation_) {
            if (completed->resources_) {
                plan_ = std::move(completed->plan_);
                plan_generation_ = generation_;
                preview_routing_.Commit();
                prepared_resources_ = std::move(completed->resources_);
                force_asset_reload_ = false;
                diagnostics_.clear();
                status_.clear();
            } else {
                std::cerr << completed->error_ << '\n';
                diagnostics_ = {{"asset.prepare_failed"}};
                status_ = Text("asset.prepare_failed");
            }
        }
        host.ClearViewerTextures();
        const bool seekable =
                plan_ &&
                std::none_of(
                        plan_->instructions_.begin(), plan_->instructions_.end(),
                        [](const auto& instruction) {
                            return instruction.operation_ == graph::Operation::kFeedback ||
                                   instruction.operation_ == graph::Operation::kTextureTrail ||
                                   instruction.operation_ == graph::Operation::kParticleEmitter ||
                                   instruction.operation_ == graph::Operation::kPointPhysics ||
                                   instruction.operation_ == graph::Operation::kGpuParticleEmitter;
                        });
#ifdef RHYTHM_HAS_LOCAL_MEDIA
        soundtrack_panel_.Sync(history_->Current(), project_ / "assets", audio_panel_,
                               unavailable_assets_);
#endif
        audio_panel_.ApplyPlayback(timeline_.TakePlaybackCommand());
        const auto audio_frame = audio_panel_.Frame();
        const auto playback_seconds = timeline_.Advance(seconds, seekable, audio_frame.playback_);
        const bool transport_changed = timeline_generation_ != timeline_.Generation();
        if (transport_changed) {
            timeline_generation_ = timeline_.Generation();
            ++reset_;
        }
        const parameters::ControlBank empty_bank;
        if (auto controls = beat_performance_.Advance(
                    {playback_seconds, timeline_.Generation(), timeline_.Paused()}, generation_,
                    history_->Current().document_.beat_grid_, plan_ ? plan_->controls_ : empty_bank,
                    plan_ && plan_generation_ == generation_ && diagnostics_.empty()))
            inspector_.PerformControls(std::move(*controls));
        // Evaluate/capture before building UI draw lists. Preview owners remain
        // alive through submission, even when this frame changes viewer demand.
        if (preview_routing_.TakeInvalidation()) {
            viewers_.BeginFrame(seconds, false, reset_);
            signal_previews_.Clear();
        }
        const auto active_viewers = preview_routing_.ActiveNodes();
        const auto viewer_due = viewers_.BeginFrame(seconds, !active_viewers.empty(), reset_);
        runtime::FrameResult output;
        if (plan_) {
            const render::Extent extent{static_cast<std::uint16_t>(plan_->canvas_.width_),
                                        static_cast<std::uint16_t>(plan_->canvas_.height_)};
            runtime::FrameContext frame{playback_seconds, reset_, extent, viewer_due};
            frame.resources_ = prepared_resources_->models_;
            frame.images_ = prepared_resources_->images_;
            frame.shaders_ = prepared_resources_->shaders_;
            frame.videos_ = videos_.Update(*plan_, *prepared_resources_, playback_seconds, reset_);
            if (!videos_.Error().empty()) status_ = Text("video.playback_failed");
            if (!timeline_.Paused() || transport_changed || audio_frame.playback_) {
                preview_inputs_ = input_preview_.Snapshot(playback_seconds);
                preview_inputs_.audio_ = audio_frame.features_;
            }
            frame.external_ = preview_inputs_;
            frame.external_.controls_ = inspector_.LiveControls(plan_->controls_);
            if (reuse_textures_)
                frame.retained_textures_ =
                        std::vector<graph::NodeId>(active_viewers.begin(), active_viewers.end());
            frame.profile_nodes_ = profiling_;
            frame.advance_state_ =
                    !timeline_.Paused() && (!audio_frame.playback_ || transport_changed ||
                                            evaluated_seconds_ != playback_seconds);
            output = runtime_.EvaluateSafely(*plan_, frame, renderer);
            evaluated_seconds_ = playback_seconds;
        }
        evaluated_ = output.evaluated_;
        recycled_textures_ = output.recycled_textures_;
        profiled_nodes_ = output.profiles_.size();
        budget_limited_ = output.budget_.has_value();
        if (output.budget_ || !show_viewers_ || active_viewers.empty()) signal_previews_.Clear();
        if (viewer_due && plan_ && !output.budget_) {
            signal_previews_.Capture(output, preview_routing_.SignalNodes(), playback_seconds,
                                     reset_);
        }
        if (output.budget_) viewers_.BeginFrame(seconds, false, reset_);
        try {
            viewers_.Capture(output, active_viewers, renderer);
        } catch (const render::BudgetExceeded&) {
            viewers_.BeginFrame(seconds, false, reset_);
            signal_previews_.Clear();
            show_viewers_ = false;
            status_ = Text("render.preview_budget");
        }
        CanvasPreviews previews;
        previews.enabled_ = show_viewers_;
        previews.signals_ = signal_previews_.Traces();
        for (const auto& value : viewers_.Outputs())
            if (renderer.IsValid(value.texture_))
                previews.textures_[value.node_] = host.RegisterTexture(value.texture_);
        const auto dock = ImGui::DockSpaceOverViewport();
        if (!layout_created_) {
            ImGui::DockBuilderRemoveNode(dock);
            ImGui::DockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dock, ImGui::GetMainViewport()->Size);
            auto center = dock;
            const auto right =
                    ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.30f, nullptr, &center);
            auto sidebar = right;
            const auto bottom =
                    ImGui::DockBuilderSplitNode(sidebar, ImGuiDir_Down, 0.46f, nullptr, &sidebar);
            ImGui::DockBuilderDockWindow("###graph", center);
            ImGui::DockBuilderDockWindow("###inspector", sidebar);
            ImGui::DockBuilderDockWindow("###output", bottom);
            ImGui::DockBuilderFinish(dock);
            layout_created_ = true;
        }
        const auto graph_visible = ImGui::Begin((Text("graph") + "###graph").c_str());
        if (graph_visible) {
            Toolbar(host, renderer, seconds);
            if (output.rejected_event_total_)
                ImGui::TextColored({1.0f, 0.55f, 0.3f, 1.0f}, "%s: %llu",
                                   Text("event.rejected_reports").c_str(),
                                   static_cast<unsigned long long>(output.rejected_event_total_));
            previews.enabled_ = show_viewers_;
            if (const auto edit = canvas_.Draw(history_->Current(), registry_,
                                               catalogs_.at(locale_), previews))
                Apply(*edit);
        }
        ImGui::End();
        std::vector<graph::NodeId> demand;
        if (graph_visible && show_viewers_) {
            const auto visible = canvas_.PreviewNodes();
            demand.assign(visible.begin(), visible.end());
            if (const auto selected = std::find(demand.begin(), demand.end(), canvas_.Selection());
                selected != demand.end())
                std::rotate(demand.begin(), selected, selected + 1);
        }
        if (demand != viewer_nodes_) {
            viewer_nodes_ = std::move(demand);
            QueueCompile();
        }
        ImGui::Begin((Text("inspector") + "###inspector").c_str());
        ImGui::BeginDisabled(timeline_.Preview().has_value());
        Inspector();
        ImGui::EndDisabled();
        audio_panel_.Draw(catalogs_.at(locale_));
#ifdef RHYTHM_HAS_LOCAL_MEDIA
        ImGui::BeginDisabled(store_.Busy());
        if (const auto action = soundtrack_panel_.Draw(history_->Current(),
                                                       audio_panel_.SelectedFile().has_value(),
                                                       catalogs_.at(locale_))) {
            CommitEdits();
            if (auto edit = soundtrack_panel_.Start(*action, history_->Current(),
                                                    project_ / "assets", audio_panel_))
                Apply(std::move(*edit));
        }
        ImGui::EndDisabled();
#endif
        input_preview_.Draw(catalogs_.at(locale_));
        ImGui::Separator();
        ImGui::Text("%s: %llu", Text("revision").c_str(),
                    static_cast<unsigned long long>(history_->Current().document_.revision_));
        ImGui::Text("%s: %u", Text("evaluated").c_str(), evaluated_);
        profiling_ = DrawPerformancePanel(output, renderer.Stats(), canvas_.Selection(),
                                          reuse_textures_, catalogs_.at(locale_));
        for (const auto& diagnostic : diagnostics_)
            ImGui::TextWrapped("%s [%llu]", Text(diagnostic.code_).c_str(),
                               static_cast<unsigned long long>(diagnostic.node_));
        ImGui::End();
        if (show_timeline_) {
            ImGui::SetNextWindowSize({760, 530}, ImGuiCond_FirstUseEver);
            if (ImGui::Begin((Text("timeline") + "###timeline").c_str(), &show_timeline_)) {
                ImGui::BeginDisabled(inspector_.Preview().has_value());
                std::optional<std::filesystem::path> music;
#ifdef RHYTHM_HAS_LOCAL_MEDIA
                music = audio_panel_.SelectedFile();
#endif
                auto edit = timeline_.Draw(
                        history_->Current(), seekable, catalogs_.at(locale_), music,
                        [&] { return history_->ReserveNodeId(); }, project_ / "assets");
                ImGui::EndDisabled();
                if (edit.committed_)
                    Apply(std::move(*edit.committed_));
                else if (edit.preview_changed_)
                    QueueCompile();
            }
            ImGui::End();
        }
        if (!show_timeline_ && timeline_.Preview()) CommitEdits();
        if (!show_timeline_) timeline_.CancelMediaPreview();
        auto component_previews = preview_routing_.Scoped(previews);
        component_previews.enabled_ = show_viewers_;
        if (const auto edited =
                    component_workbench_.Draw(history_->Current(), registry_, catalogs_.at(locale_),
                                              locale_, preview_routing_, component_previews)) {
            inspector_.Reset();
            Apply(*edited);
            canvas_.RestoreLayout();
        }
        const bool preview_page_changed = preview_routing_.TakePageChange();
        if (component_preview_generation_ != component_workbench_.PreviewGeneration() ||
            preview_page_changed) {
            component_preview_generation_ = component_workbench_.PreviewGeneration();
            QueueCompile();
        }
        const auto output_visible = ImGui::Begin((Text("output") + "###output").c_str());
        if (output_visible && output.budget_) {
            ImGui::TextWrapped("%s", Text("render.resource_budget").c_str());
            if (ImGui::Button(Label("render.retry").c_str())) ++reset_;
        }
        if (output_visible && output.final_.device_) {
            const auto available = ImGui::GetContentRegionAvail();
            const auto fit = render::AspectFit(output.extent_, {0, 0, std::max(1.0f, available.x),
                                                                std::max(1.0f, available.y)});
            const auto cursor = ImGui::GetCursorPos();
            ImGui::SetCursorPos({cursor.x + fit.x_, cursor.y + fit.y_});
            ImGui::Image(host.RegisterTexture(output.final_), {fit.width_, fit.height_});
        }
        ImGui::End();
#ifdef RHYTHM_HAS_LOCAL_MEDIA
        if (auto request = export_panel_.Draw(catalogs_.at(locale_))) {
            CommitEdits();
            export_panel_.Start(host.ResourceDirectory() / "rhythm_master.exe", history_->Current(),
                                project_ / "assets", std::move(*request));
        }
#endif
    }
    void CommitEdits() {
        auto next = inspector_.Preview()  ? *inspector_.Preview()
                    : timeline_.Preview() ? *timeline_.Preview()
                                          : history_->Current();
        next.title_ = title_.data();
        inspector_.Reset();
        timeline_.ResetEdit();
        Apply(std::move(next));
    }
    graph::Registry registry_{};
    std::vector<content::Preset> presets_{};
    std::vector<content::Semantic> semantics_{};
    SemanticPalette semantic_palette_{};
    NodePalette node_palette_{};
    std::optional<editor::History> history_{};
    editor::CompilerWorker compiler_{};
    project::AsyncStore store_{};
    prepared_assets::Loader asset_loader_{};
    bool force_asset_reload_ = false;
    std::vector<assets::AssetId> unavailable_assets_{};
    std::shared_ptr<const prepared_assets::Resources> prepared_resources_ =
            std::make_shared<const prepared_assets::Resources>();
    AssetPanel assets_{};
    ShaderPanel shader_panel_{};
    std::optional<graph::ExecutionPlan> plan_{};
    runtime::Runtime runtime_{};
    video_sources::Streams videos_{};
    runtime::Viewers viewers_{};
    runtime::SignalPreviews signal_previews_{};
    std::vector<graph::NodeId> viewer_nodes_{};
    std::uint32_t evaluated_ = 0;
    GraphCanvas canvas_{};
    std::vector<graph::Diagnostic> diagnostics_{};
    std::map<std::string, std::map<std::string, std::string>> catalogs_{};
    std::filesystem::path project_{};
    std::vector<project::ContentEntry> templates_{};
    TemplateBrowser template_browser_{};
    std::string locale_ = "zh-CN";
    std::string status_{};
    std::array<char, 4097> title_{};
    PropertyInspector inspector_{};
    BeatPerformance beat_performance_{};
    ComponentPanel component_panel_{};
    ComponentLibraryPanel component_library_{};
    ComponentWorkbench component_workbench_{};
    std::uint64_t component_preview_generation_ = 0;
    PreviewRouting preview_routing_{};
    InputPreview input_preview_{};
    audio_ui::AudioPanel audio_panel_{};
    TimelinePanel timeline_{};
#ifdef RHYTHM_HAS_LOCAL_MEDIA
    ExportPanel export_panel_{};
    SoundtrackPanel soundtrack_panel_{};
#endif
    runtime::ExternalInputs preview_inputs_{};
    std::optional<double> evaluated_seconds_{};
    std::uint64_t timeline_generation_ = 0;
    std::uint64_t generation_ = 0;
    std::uint64_t plan_generation_ = 0;
    std::uint64_t reset_ = 0;
    std::uint64_t load_revision_ = 0;
    bool show_viewers_ = true;
    bool budget_limited_ = false;
    bool profiling_ = false;
    bool reuse_textures_ = true;
    std::uint32_t recycled_textures_ = 0;
    std::size_t profiled_nodes_ = 0;
    bool show_timeline_ = false;
    bool layout_created_ = false;
};
Studio::Studio(const std::filesystem::path& resources, const std::filesystem::path& project)
    : impl_(std::make_unique<Impl>(resources, project)) {}
Studio::~Studio() = default;
void Studio::Frame(platform::Host& host, render::Renderer& renderer, double seconds) {
    impl_->Frame(host, renderer, seconds);
}
bool Studio::HasValidPlan() const {
    return impl_->plan_.has_value() && impl_->plan_generation_ == impl_->generation_ &&
           impl_->diagnostics_.empty();
}
void Studio::SetTextureReuse(bool enabled) { impl_->reuse_textures_ = enabled; }
void Studio::SetSuspended(bool suspended) { impl_->audio_panel_.SetSuspended(suspended); }
FrameStatus Studio::Status() const {
    return {impl_->history_->Current().document_.nodes_.size(),
            impl_->canvas_.VisibleNodes(),
            impl_->viewers_.Outputs().size(),
            impl_->canvas_.DrawnPreviews(),
            impl_->preview_inputs_.audio_ ? impl_->preview_inputs_.audio_->rms_ : 0,
            impl_->budget_limited_,
            impl_->recycled_textures_,
            impl_->profiled_nodes_,
            impl_->component_workbench_.DrawnPreviews(),
            impl_->signal_previews_.Traces().size(),
            impl_->timeline_.WaveformBins(),
            impl_->preview_routing_.Page(),
            impl_->preview_routing_.Pages(),
            impl_->timeline_.ClipWaveformSources()};
}
WorkflowStatus Studio::Workflow() const {
    WorkflowStatus result;
    result.requested_generation_ = impl_->generation_;
    result.installed_generation_ = impl_->plan_generation_;
    for (const auto& diagnostic : impl_->diagnostics_)
        result.graph_errors_.push_back(diagnostic.code_);
#ifdef RHYTHM_HAS_LOCAL_MEDIA
    const auto job = impl_->export_panel_.Snapshot();
    static constexpr std::array states{"idle",     "preparing", "rendering", "publishing",
                                       "complete", "canceled",  "failed"};
    static constexpr std::array phases{"idle",       "preparing",  "launching", "rendering",
                                       "finalizing", "publishing", "complete"};
    result.export_state_ = states.at(static_cast<std::size_t>(job.state_));
    result.export_phase_ = phases.at(static_cast<std::size_t>(job.phase_));
    result.exported_frames_ = job.progress_.completed_frames_;
    result.export_total_frames_ = job.progress_.total_frames_;
    result.export_error_ =
            impl_->export_panel_.Error().empty() ? job.error_ : impl_->export_panel_.Error();
#endif
    return result;
}
void Studio::LoadAudioFile(const std::filesystem::path& path, float volume) {
#ifdef RHYTHM_HAS_LOCAL_MEDIA
    impl_->audio_panel_.SetVolume(volume);
    impl_->audio_panel_.LoadFile(path);
#else
    static_cast<void>(path);
    static_cast<void>(volume);
    throw std::runtime_error("media.disabled");
#endif
}
}  // namespace rhythm::studio
