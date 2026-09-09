#include "shader_panel.h"

#include <imgui.h>

#include <algorithm>
#include <utility>

namespace rhythm::studio {
namespace {
bool UsesAsset(std::span<const graph::Node> nodes, const assets::AssetId& id) {
    for (const auto& node : nodes)
        for (const auto& [key, value] : node.properties_)
            if (std::holds_alternative<assets::AssetId>(value) &&
                std::get<assets::AssetId>(value) == id)
                return true;
    return false;
}
}  // namespace
std::optional<editor::Snapshot> ShaderPanel::Take(const editor::Snapshot& snapshot) {
    const auto result = compiler_.Take();
    if (!result) return {};
    const auto pending = std::exchange(pending_, {});
    if (!pending || pending->document_ != snapshot.document_.id_) return {};
    const auto found =
            std::find_if(snapshot.document_.nodes_.begin(), snapshot.document_.nodes_.end(),
                         [&](const auto& node) { return node.id_ == pending->node_; });
    if (found == snapshot.document_.nodes_.end() || found->type_ != "texture.shader" ||
        std::get<assets::AssetId>(found->properties_.at("asset")) != pending->asset_) {
        error_ = "shader.stale";
        return {};
    }
    error_ = result->error_;
    diagnostic_ = result->diagnostic_;
    if (!result->asset_) return {};
    auto next = snapshot;
    if (std::none_of(next.assets_.begin(), next.assets_.end(),
                     [&](const auto& record) { return record.id_ == result->asset_->id_; }))
        next.assets_.push_back(*result->asset_);
    for (auto& node : next.document_.nodes_)
        if (node.id_ == pending->node_) node.properties_["asset"] = result->asset_->id_;
    // Recompiling must not fill the publication budget with obsolete versions.
    // Historical snapshots and immutable blobs still retain undo's prior asset.
    bool used = UsesAsset(next.document_.nodes_, pending->asset_) ||
                (next.soundtrack_ && next.soundtrack_->asset_ == pending->asset_);
    for (const auto& component : next.document_.components_)
        used |= UsesAsset(component.nodes_, pending->asset_);
    if (!used)
        std::erase_if(next.assets_,
                      [&](const auto& record) { return record.id_ == pending->asset_; });
    if (document_ == pending->document_ && node_ == pending->node_) {
        asset_ = result->asset_->id_;
        loaded_ = true;
        source_loader_.Cancel();
        ++load_generation_;
    }
    edited_ = false;
    return next;
}
void ShaderPanel::Draw(const editor::Snapshot& snapshot, graph::NodeId selected,
                       const std::filesystem::path& assets,
                       const std::map<std::string, std::string>& text) {
    const auto found =
            std::find_if(snapshot.document_.nodes_.begin(), snapshot.document_.nodes_.end(),
                         [&](const auto& node) { return node.id_ == selected; });
    if (found == snapshot.document_.nodes_.end() || found->type_ != "texture.shader") return;
    const auto& id = std::get<assets::AssetId>(found->properties_.at("asset"));
    if (document_ != snapshot.document_.id_ || node_ != selected || asset_ != id) {
        document_ = snapshot.document_.id_;
        node_ = selected;
        asset_ = id;
        loaded_ = edited_ = false;
        source_loader_.Cancel();
        ++load_generation_;
        buffer_.fill(0);
        error_.clear();
        diagnostic_.reset();
        const std::string initial =
                "vec4(0.5 + 0.5 * cos(time + uv.xyx * 6.283185 + vec3(0.0, 2.0, 4.0)), 1.0)";
        if (id.sha256_.empty()) {
            std::copy(initial.begin(), initial.end(), buffer_.begin());
            loaded_ = true;
        } else {
            // Source editing must also work for disconnected/unreachable nodes.
            // Prepare just this shader asset independently of the running graph.
            graph::Document source_document;
            source_document.id_ = document_;
            const graph::Registry registry;
            const graph::NodeId output = found->id_ == 1 ? 2 : 1;
            source_document.nodes_ = {*found, registry.MakeNode(output, "output.texture")};
            source_document.edges_ = {{1, found->id_, output, "source"}};
            source_document.output_ = output;
            const auto compiled = graph::Compile(source_document, graph::Registry{});
            if (std::holds_alternative<graph::ExecutionPlan>(compiled))
                source_loader_.Submit({std::get<graph::ExecutionPlan>(compiled), snapshot.assets_,
                                       assets, load_generation_});
            else {
                loaded_ = true;
                error_ = "shader.read";
            }
        }
    }
    if (const auto result = source_loader_.Take();
        result && result->generation_ == load_generation_) {
        loaded_ = true;
        error_ = result->error_;
        if (result->resources_ && !edited_) {
            const auto& expression =
                    result->resources_->shaders_->programs_.at(id.sha256_).expression_;
            buffer_.fill(0);
            std::copy_n(expression.begin(), std::min(expression.size(), buffer_.size() - 1),
                        buffer_.begin());
        }
    }
    ImGui::SeparatorText(text.at("shader.editor").c_str());
    ImGui::TextWrapped("%s", text.at("shader.help").c_str());
    if (!loaded_) ImGui::TextUnformatted(text.at("shader.loading").c_str());
    ImGui::BeginDisabled(compiler_.Busy() || !loaded_);
    if (ImGui::InputTextMultiline("###shader.source", buffer_.data(), buffer_.size(),
                                  ImVec2(-1, 180)))
        edited_ = true;
    if (ImGui::Button((text.at("shader.compile") + "###shader.compile").c_str())) {
        error_.clear();
        diagnostic_.reset();
        if (compiler_.Start({tools_, assets, buffer_.data()}))
            pending_ = Pending{document_, node_, asset_};
    }
    ImGui::EndDisabled();
    if (compiler_.Busy()) {
        ImGui::SameLine();
        if (ImGui::Button(text.at("cancel").c_str())) compiler_.Cancel();
        ImGui::TextUnformatted(text.at("shader.compiling").c_str());
    }
    if (!error_.empty()) {
        const auto translated = text.find(error_);
        ImGui::PushTextWrapPos(0);
        ImGui::TextUnformatted((translated == text.end() ? error_ : translated->second).c_str());
        if (diagnostic_)
            ImGui::Text("%s %u : %u", text.at("shader.location").c_str(), diagnostic_->line_,
                        diagnostic_->column_);
        ImGui::PopTextWrapPos();
    }
}
}  // namespace rhythm::studio
