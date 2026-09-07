#include "node_visual.h"

#include <imgui.h>
#include <imgui_node_editor.h>

#include <algorithm>

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
    }
    return {170, 180, 195};
}

void DrawPort(const PortVisual& port, ed::PinKind kind) {
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
    if (port.bound_ || ed::PinHadAnyLinks(ed::PinId(port.id_)))
        ImGui::GetWindowDrawList()->AddCircleFilled(center, 5, color, 16);
    else
        ImGui::GetWindowDrawList()->AddCircle(center, 5, color, 16, 2);
    ed::EndPin();
}
}  // namespace

PreviewBounds DrawNode(const NodeVisual& node) {
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
    ImGui::Dummy({width, header_height});
    ImGui::GetWindowDrawList()->AddText({origin.x + 4, origin.y + 4}, IM_COL32(240, 244, 250, 255),
                                        node.title_.c_str());
    const auto body_y = ImGui::GetCursorScreenPos().y;
    for (std::size_t row = 0; row < rows; ++row) {
        const auto y = body_y + float(row) * row_height;
        if (row < node.inputs_.size()) {
            const auto& port = node.inputs_[row];
            ImGui::SetCursorScreenPos({origin.x, y});
            DrawPort(port, ed::PinKind::Input);
            ImGui::SameLine();
            ImGui::TextUnformatted(port.label_.c_str());
        }
        if (row == 0) {
            ImGui::SetCursorScreenPos(
                    {origin.x + width - output_width - 18 - ImGui::GetStyle().ItemSpacing.x, y});
            ImGui::TextUnformatted(node.output_.label_.c_str());
            ImGui::SameLine();
            DrawPort(node.output_, ed::PinKind::Output);
        }
    }
    ImGui::SetCursorScreenPos({origin.x, body_y + float(rows) * row_height});
    PreviewBounds preview;
    if (node.preview_enabled_) {
        const auto position = ImGui::GetCursorScreenPos();
        preview = {position.x, position.y, width, width * 0.5625f};
        if (node.preview_texture_)
            ImGui::Image(node.preview_texture_, {preview.width_, preview.height_});
        else {
            ImGui::Dummy({preview.width_, preview.height_});
            ImGui::GetWindowDrawList()->AddRectFilled(
                    position, {position.x + preview.width_, position.y + preview.height_},
                    IM_COL32(16, 22, 31, 255), 4);
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
    ed::Link(ed::LinkId(id), ed::PinId(from), ed::PinId(to), TypeColor(type), 2.0f);
}
}  // namespace rhythm::studio
