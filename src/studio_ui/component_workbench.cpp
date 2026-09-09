#include "component_workbench.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>

#include "preview_routing.h"
#include "rhythm/runtime/viewers.h"

namespace rhythm::studio {
bool ComponentWorkbench::OpenAuthor(const editor::Snapshot& project,
                                    const graph::AuthorNode& author) {
    if (author.instance_path_.empty()) return false;
    const auto& nodes = project.document_.nodes_;
    const auto root = std::find_if(nodes.begin(), nodes.end(), [&](const auto& node) {
        return node.id_ == author.instance_path_.front();
    });
    if (root == nodes.end()) return false;
    try {
        editor::ComponentEdit candidate(project, root->type_);
        for (std::size_t index = 1; index < author.instance_path_.size(); ++index)
            if (!candidate.Enter(author.instance_path_[index])) return false;
        const auto body = candidate.Body();
        if (!std::any_of(body.document_.nodes_.begin(), body.document_.nodes_.end(),
                         [&](const auto& node) { return node.id_ == author.node_; }))
            return false;
        edit_ = std::move(candidate);
        instance_path_ = author.instance_path_;
        preview_body_.reset();
        preview_document_.reset();
        preview_nodes_.clear();
        ++preview_generation_;
        status_.clear();
        ResetView();
        canvas_.Select(author.node_);
        canvas_.FocusSelection();
        return true;
    } catch (const std::exception&) {
        return false;
    }
}
void ComponentWorkbench::Open(const editor::Snapshot& project, std::string type,
                              graph::NodeId instance) {
    instance_path_.clear();
    const auto& nodes = project.document_.nodes_;
    auto found = std::find_if(nodes.begin(), nodes.end(), [&](const auto& node) {
        return node.id_ == instance && node.type_ == type;
    });
    if (found == nodes.end())
        found = std::find_if(nodes.begin(), nodes.end(),
                             [&](const auto& node) { return node.type_ == type; });
    if (found != nodes.end()) instance_path_.push_back(found->id_);
    edit_.emplace(project, std::move(type));
    preview_body_.reset();
    preview_document_.reset();
    preview_nodes_.clear();
    ++preview_generation_;
    status_.clear();
    ResetView();
}
void ComponentWorkbench::CommitPreview() {
    if (edit_ && inspector_.Preview()) edit_->ReplaceBody(*inspector_.Preview());
    if (edit_ && timing_.Preview()) edit_->ReplaceBody(*timing_.Preview());
    inspector_.Reset();
    timing_.Reset();
}
void ComponentWorkbench::ResetView() {
    inspector_.Reset();
    timing_.Reset();
    canvas_.RestoreLayout();
    if (edit_ && instance_path_.size() > edit_->Path().size())
        instance_path_.resize(edit_->Path().size());
}
void ComponentWorkbench::UpdatePreview(const editor::Snapshot& project,
                                       const graph::Registry& registry, bool visible) {
    if (!edit_) {
        if (preview_document_ || !preview_nodes_.empty()) ++preview_generation_;
        preview_document_.reset();
        preview_body_.reset();
        preview_nodes_.clear();
        return;
    }
    const auto body = inspector_.Preview() ? *inspector_.Preview()
                      : timing_.Preview()  ? *timing_.Preview()
                                           : edit_->Body();
    if (instance_path_.size() > edit_->Path().size()) instance_path_.resize(edit_->Path().size());
    if (!preview_body_ || *preview_body_ != body.document_ ||
        source_revision_ != project.document_.revision_ || source_id_ != project.document_.id_) {
        auto preview_edit = *edit_;
        if (inspector_.Preview() || timing_.Preview()) preview_edit.ReplaceBody(body);
        const auto finished = preview_edit.Finish(project, registry);
        if (std::holds_alternative<editor::Snapshot>(finished)) {
            preview_document_ = std::get<editor::Snapshot>(finished).document_;
        } else {
            preview_document_.reset();
            status_ = std::get<graph::Diagnostic>(finished).code_;
        }
        preview_body_ = body.document_;
        source_revision_ = project.document_.revision_;
        source_id_ = project.document_.id_;
        ++preview_generation_;
    }
    std::vector<graph::NodeId> nodes;
    if (visible && preview_document_ && !instance_path_.empty()) {
        const auto shown = canvas_.PreviewNodes();
        nodes.assign(shown.begin(), shown.end());
        if (const auto selected = std::find(nodes.begin(), nodes.end(), canvas_.Selection());
            selected != nodes.end())
            std::rotate(nodes.begin(), selected, selected + 1);
    }
    if (nodes != preview_nodes_ || preview_path_ != instance_path_) {
        preview_nodes_ = std::move(nodes);
        preview_path_ = instance_path_;
        ++preview_generation_;
    }
}
std::optional<editor::Snapshot> ComponentWorkbench::Draw(
        const editor::Snapshot& project, const graph::Registry& registry,
        const std::map<std::string, std::string>& text, const std::string& locale,
        PreviewRouting& routing, const CanvasPreviews& previews) {
    if (!edit_) return {};
    const auto label = [&](const std::string& key) {
        const auto found = text.find(key);
        return found == text.end() ? key : found->second;
    };
    bool open = true;
    bool canvas_visible = false;
    const auto drawn_path = instance_path_;
    std::optional<editor::Snapshot> result;
    ImGui::SetNextWindowSize({1100, 750}, ImGuiCond_FirstUseEver);
    if (ImGui::Begin((label("component.edit") + "###component.workbench").c_str(), &open)) {
        ImGui::TextWrapped("%s", label("component.draft_help").c_str());
        if (previews.enabled_) routing.DrawNavigation(text);
        if (ImGui::Button(label("component.apply").c_str())) {
            CommitPreview();
            auto applied = edit_->Finish(project, registry);
            if (std::holds_alternative<editor::Snapshot>(applied)) {
                result = std::get<editor::Snapshot>(std::move(applied));
                open = false;
            } else
                status_ = label(std::get<graph::Diagnostic>(applied).code_);
        }
        ImGui::SameLine();
        if (ImGui::Button(label("component.cancel").c_str())) open = false;
        ImGui::SameLine();
        if (ImGui::Button(label("undo").c_str())) {
            CommitPreview();
            edit_->Undo();
            ResetView();
        }
        ImGui::SameLine();
        if (ImGui::Button(label("redo").c_str())) {
            CommitPreview();
            edit_->Redo();
            ResetView();
        }
        const auto path = edit_->Path();
        for (std::size_t index = 0; index < path.size(); ++index) {
            ImGui::SameLine();
            if (ImGui::SmallButton(
                        (label(path[index]) + "###crumb" + std::to_string(index)).c_str())) {
                CommitPreview();
                edit_->Navigate(index);
                if (instance_path_.size() > index + 1) instance_path_.resize(index + 1);
                ResetView();
                break;
            }
        }
        if (!status_.empty()) ImGui::TextWrapped("%s", label(status_).c_str());
        if (ImGui::Button(label("component.enter").c_str())) {
            CommitPreview();
            const auto selected = canvas_.Selection();
            if (edit_->Enter(selected)) {
                if (!instance_path_.empty()) instance_path_.push_back(selected);
                ResetView();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(label("component.set_output").c_str())) {
            CommitPreview();
            auto body = edit_->Body();
            if (std::any_of(body.document_.nodes_.begin(), body.document_.nodes_.end(),
                            [&](const auto& node) { return node.id_ == canvas_.Selection(); })) {
                body.document_.output_ = canvas_.Selection();
                edit_->ReplaceBody(std::move(body));
            }
        }
        ImGui::SameLine();
        std::vector<graph::OperatorDescriptor> available_nodes(registry.Operators().begin(),
                                                               registry.Operators().end());
        const auto body = edit_->Body();
        for (const auto& definition : body.document_.components_) {
            if (std::find(edit_->Path().begin(), edit_->Path().end(), definition.type_) !=
                edit_->Path().end())
                continue;
            if (auto descriptor = registry.Find(definition.type_, body.document_.components_))
                available_nodes.push_back(std::move(*descriptor));
        }
        if (const auto type = node_palette_.Draw(available_nodes, text, "component.add")) {
            CommitPreview();
            const auto id = edit_->ReserveNodeId();
            auto added =
                    editor::AddNode(edit_->Body(), registry, *type, canvas_.InsertionPoint(), id);
            if (std::holds_alternative<editor::Snapshot>(added)) {
                edit_->ReplaceBody(std::get<editor::Snapshot>(std::move(added)));
                ResetView();
                canvas_.Select(id);
            } else {
                status_ = label(std::get<graph::Diagnostic>(added).code_);
            }
        }
        ImGui::SameLine();
        ImGui::Text("%s: %llu", label("output").c_str(),
                    static_cast<unsigned long long>(edit_->Definition().output_));
        if (instance_path_.empty())
            ImGui::TextWrapped("%s", label("component.preview_no_instance").c_str());
        else {
            std::string instance_label;
            for (const auto id : instance_path_) instance_label += "/" + std::to_string(id);
            ImGui::Text("%s: %s", label("component.preview_instance").c_str(),
                        instance_label.c_str());
        }
        const auto available = ImGui::GetContentRegionAvail();
        if (ImGui::BeginChild("component.canvas", {std::max(250.0f, available.x * 0.62f), 0})) {
            canvas_visible = true;
            ImGui::BeginDisabled(inspector_.Preview().has_value() || timing_.Preview().has_value());
            const auto current_previews =
                    drawn_path == instance_path_ ? previews : CanvasPreviews{previews.enabled_, {}};
            if (const auto edited = canvas_.Draw(edit_->Body(), registry, text, current_previews)) {
                edit_->ReplaceBody(*edited);
            }
            ImGui::EndDisabled();
        }
        ImGui::EndChild();
        ImGui::SameLine();
        if (ImGui::BeginChild("component.inspector")) {
            ImGui::BeginDisabled(inspector_.Preview().has_value());
            if (ImGui::CollapsingHeader(
                        (label("component.timing") + "###component.timing").c_str())) {
                DrawTiming(text);
            } else if (timing_.Preview()) {
                CommitPreview();
            }
            ImGui::EndDisabled();
            ImGui::BeginDisabled(timing_.Preview().has_value());
            const auto inspected =
                    inspector_.Draw(edit_->Body(), canvas_.Selection(), registry, {}, text, locale);
            if (inspected.committed_) edit_->ReplaceBody(*inspected.committed_);
            if (const auto definition =
                        interface_.Draw(edit_->Definition(), edit_->Body().document_,
                                        canvas_.Selection(), registry, text)) {
                CommitPreview();
                edit_->ReplaceInterface(*definition);
            }
            ImGui::EndDisabled();
        }
        ImGui::EndChild();
    }
    ImGui::End();
    if (!open) {
        edit_.reset();
        inspector_.Reset();
        timing_.Reset();
    }
    UpdatePreview(project, registry, canvas_visible && previews.enabled_);
    return result;
}
void ComponentWorkbench::DrawTiming(const std::map<std::string, std::string>& text) {
    ImGui::TextWrapped("%s", text.at("component.timing_help").c_str());
    ImGui::BeginDisabled(timing_.Preview().has_value());
    const double minimum = 0, maximum = 86400, duration_minimum = 0.01;
    ImGui::SetNextItemWidth(130);
    ImGui::DragScalar(text.at("component.timing_range").c_str(), ImGuiDataType_Double,
                      &timing_duration_, 0.1F, &duration_minimum, &maximum, "%.2f s",
                      ImGuiSliderFlags_AlwaysClamp);
    timing_duration_ = std::isfinite(timing_duration_)
                               ? std::clamp(timing_duration_, duration_minimum, maximum)
                               : 16;
    ImGui::SetNextItemWidth(130);
    ImGui::DragScalar(text.at("component.timing_insert").c_str(), ImGuiDataType_Double,
                      &timing_insert_, 0.1F, &minimum, &maximum, "%.2f s",
                      ImGuiSliderFlags_AlwaysClamp);
    timing_insert_ =
            std::isfinite(timing_insert_) ? std::clamp(timing_insert_, minimum, maximum) : 0;
    ImGui::EndDisabled();
    const auto changed = timing_.Draw(
            edit_->Body(), timing_insert_, timing_duration_, text,
            [&] { return edit_->ReserveNodeId(); }, "component.add_section");
    if (changed.committed_) edit_->ReplaceBody(*changed.committed_);
}
}  // namespace rhythm::studio
