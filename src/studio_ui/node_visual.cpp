#include "node_visual.h"

#include <imgui.h>
#include <imgui_node_editor.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace rhythm::studio {
namespace ed = ax::NodeEditor;
namespace {
ImColor TypeColor(graph::ValueType type) {
    switch (type) {
        case graph::ValueType::kScalar:
            return {232, 188, 94};
        case graph::ValueType::kSignal:
            return {116, 211, 146};
        case graph::ValueType::kTexture:
            return {77, 180, 223};
        case graph::ValueType::kGpuPoints:
            return {255, 101, 173};
        case graph::ValueType::kPoints:
            return {214, 129, 226};
        case graph::ValueType::kGeometry:
            return {114, 205, 175};
        case graph::ValueType::kMaterial:
            return {228, 142, 106};
        case graph::ValueType::kScene:
            return {146, 157, 239};
        case graph::ValueType::kCamera:
            return {229, 206, 126};
        case graph::ValueType::kSceneImage:
            return {145, 205, 239};
        case graph::ValueType::kPath:
            return {120, 228, 204};
        case graph::ValueType::kDepth:
            return {177, 189, 199};
        case graph::ValueType::kEvent:
            return {255, 150, 87};
    }
    return {170, 180, 195};
}

void DrawPort(const PortVisual& port, ed::PinKind kind, bool details) {
    ed::BeginPin(ed::PinId(port.id_), kind);
    ed::PinPivotAlignment({0.5f, 0.5f});
    // Only the visible socket starts a connection; adjacent text remains a
    // node drag area. Dummy reserves space without capturing mouse input.
    const auto origin = ImGui::GetCursorScreenPos();
    const auto height = ImGui::GetTextLineHeight() + 6;
    ImGui::Dummy({18, height});
    ed::PinRect(origin, {origin.x + 18, origin.y + height});
    const ImVec2 center{origin.x + 9, origin.y + height * 0.5f};
    const auto color = TypeColor(port.type_);
    if (!details) {
        // Preserve pin bounds and link/drag interaction at overview scale.
        // Subpixel socket circles would only create invisible triangles.
    } else if (port.bound_ || ed::PinHadAnyLinks(ed::PinId(port.id_)))
        ImGui::GetWindowDrawList()->AddCircleFilled(center, 5, color, 16);
    else
        ImGui::GetWindowDrawList()->AddCircle(center, 5, color, 16, 2);
    ed::EndPin();
}
}  // namespace

PreviewBounds DrawNode(const NodeVisual& node) {
    // This upstream API returns the canvas inverse scale (GetView().InvScale).
    const bool readable = ImGui::GetFontSize() / ed::GetCurrentZoom() >= 6.0f;
    // Capture the canvas clip before BeginNode replaces it with the node clip.
    const auto clip_min = ImGui::GetWindowDrawList()->GetClipRectMin();
    const auto clip_max = ImGui::GetWindowDrawList()->GetClipRectMax();
    float input_width = 0;
    for (const auto& port : node.inputs_)
        input_width = std::max(input_width, ImGui::CalcTextSize(port.label_.c_str()).x);
    const auto output_width = ImGui::CalcTextSize(node.output_.label_.c_str()).x;
    const auto width = std::max({200.0f, ImGui::CalcTextSize(node.title_.c_str()).x + 12,
                                 input_width + output_width + 82});
    const auto header_height = ImGui::GetTextLineHeight() + 16;
    const auto row_height = ImGui::GetTextLineHeight() + 6 + ImGui::GetStyle().ItemSpacing.y;
    const auto rows = std::max(std::size_t{1}, node.inputs_.size());
    ed::BeginNode(ed::NodeId(node.id_));
    const auto origin = ImGui::GetCursorScreenPos();
    const auto height = header_height + float(rows) * row_height +
                        (node.preview_enabled_ ? width * 0.5625f : 0.0f) + 16;
    const bool details = readable && origin.x + width + 16 >= clip_min.x &&
                         origin.y + height >= clip_min.y && origin.x - 16 <= clip_max.x &&
                         origin.y - 16 <= clip_max.y;
    const auto draw_label = [details](const std::string& text) {
        if (details)
            ImGui::TextUnformatted(text.c_str());
        else
            ImGui::Dummy(ImGui::CalcTextSize(text.c_str()));
    };
    ImGui::Dummy({width, header_height});
    if (details)
        ImGui::GetWindowDrawList()->AddText({origin.x + 4, origin.y + 4},
                                            IM_COL32(240, 244, 250, 255), node.title_.c_str());
    const auto body_y = ImGui::GetCursorScreenPos().y;
    for (std::size_t row = 0; row < rows; ++row) {
        const auto y = body_y + float(row) * row_height;
        if (row < node.inputs_.size()) {
            const auto& port = node.inputs_[row];
            ImGui::SetCursorScreenPos({origin.x, y});
            DrawPort(port, ed::PinKind::Input, details);
            ImGui::SameLine();
            draw_label(port.label_);
        }
        if (row == 0) {
            ImGui::SetCursorScreenPos(
                    {origin.x + width - output_width - 18 - ImGui::GetStyle().ItemSpacing.x, y});
            draw_label(node.output_.label_);
            ImGui::SameLine();
            DrawPort(node.output_, ed::PinKind::Output, details);
        }
    }
    ImGui::SetCursorScreenPos({origin.x, body_y + float(rows) * row_height});
    PreviewBounds preview;
    if (node.preview_enabled_) {
        const auto position = ImGui::GetCursorScreenPos();
        preview = {position.x, position.y, width, width * 0.5625f};
        if (node.preview_signal_ && node.preview_signal_->count_) {
            const auto& trace = *node.preview_signal_;
            const auto samples = std::span(trace.samples_).first(trace.count_);
            const auto [minimum, maximum] = std::minmax_element(samples.begin(), samples.end());
            const auto padding =
                    std::max(0.01f, std::max(std::abs(*minimum), std::abs(*maximum)) * 0.05f);
            char label[64]{};
            if (trace.event_observation_)
                std::snprintf(label, sizeof(label), "%.0f | #%llu @ %.3fs", trace.value_,
                              static_cast<unsigned long long>(trace.event_observation_->count_),
                              trace.event_observation_->last_seconds_);
            else
                std::snprintf(label, sizeof(label), "%.6g", trace.value_);
            ImGui::PushID(static_cast<int>(node.id_));
            ImGui::PushStyleColor(
                    trace.event_observation_ ? ImGuiCol_PlotHistogram : ImGuiCol_PlotLines,
                    TypeColor(node.output_.type_).Value);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(16, 22, 31, 255));
            // Upstream PlotLines uses ButtonBehavior. A display-only plot must
            // not claim clicks from the node editor's body drag interaction.
            ImGui::PushStyleVar(ImGuiStyleVar_DisabledAlpha, 1.0f);
            ImGui::BeginDisabled();
            ImGui::SetNextItemAllowOverlap();
            if (trace.event_observation_)
                ImGui::PlotHistogram("##event", samples.data(), static_cast<int>(samples.size()),
                                     static_cast<int>(trace.offset_), label, 0,
                                     std::max(1.0f, *maximum + padding),
                                     {preview.width_, preview.height_});
            else
                ImGui::PlotLines("##signal", samples.data(), static_cast<int>(samples.size()),
                                 static_cast<int>(trace.offset_), label, *minimum - padding,
                                 *maximum + padding, {preview.width_, preview.height_});
            ImGui::EndDisabled();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);
            ImGui::PopID();
        } else if (node.preview_texture_)
            ImGui::Image(node.preview_texture_, {preview.width_, preview.height_});
        else {
            ImGui::Dummy({preview.width_, preview.height_});
            ImGui::GetWindowDrawList()->AddRectFilled(
                    position, {position.x + preview.width_, position.y + preview.height_},
                    IM_COL32(16, 22, 31, 255), 4);
            if (details)
                ImGui::GetWindowDrawList()->AddText({position.x + 8, position.y + 8},
                                                    IM_COL32(143, 159, 179, 255),
                                                    node.preview_waiting_.c_str());
        }
    }
    ImGui::Dummy({width, 2});
    ed::EndNode();
    const auto padding = ed::GetStyle().NodePadding;
    const auto accent = TypeColor(node.output_.type_).Value;
    const ImVec2 first{origin.x - padding.x, origin.y - padding.y};
    const ImVec2 last{origin.x + width + padding.z, origin.y + header_height};
    ed::GetNodeBackgroundDrawList(ed::NodeId(node.id_))
            ->AddRectFilled(first, last,
                            ImColor(accent.x * 0.34f, accent.y * 0.34f, accent.z * 0.34f, 1.0f),
                            ed::GetStyle().NodeRounding, ImDrawFlags_RoundCornersTop);
    ed::GetNodeBackgroundDrawList(ed::NodeId(node.id_))
            ->AddLine({first.x, last.y}, last, ImColor(accent.x, accent.y, accent.z, 0.65f), 1.5f);
    return preview;
}
void DrawLink(std::uint64_t id, std::uint64_t from, std::uint64_t to, graph::ValueType type) {
    // Node-editor scales link width with the canvas. Large authored graphs can
    // fit below 10% zoom, where a fixed two-unit line becomes invisible while
    // the nodes remain discernible. Keep a readable screen-space minimum.
    const auto zoom = std::max(ed::GetCurrentZoom(), 0.08f);
    ed::Link(ed::LinkId(id), ed::PinId(from), ed::PinId(to), TypeColor(type), 2.0f / zoom);
}
}  // namespace rhythm::studio
