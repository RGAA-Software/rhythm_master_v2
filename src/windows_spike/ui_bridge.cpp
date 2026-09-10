#include "ui_bridge.h"

#include <imgui.h>

#include <stdexcept>

namespace rhythm::platform {
render::DrawList TranslateUi(const std::map<std::uint64_t, render::TextureHandle>& textures) {
    // ImGui owns draw data; consume it synchronously into project-owned values.
    const auto* data = ImGui::GetDrawData();
    if (!data) throw std::logic_error("ui.no_draw_data");
    render::DrawList list;
    const auto scale = data->FramebufferScale;
    list.width_ = data->DisplaySize.x * scale.x;
    list.height_ = data->DisplaySize.y * scale.y;
    list.vertices_.reserve(static_cast<std::size_t>(data->TotalVtxCount));
    list.indices_.reserve(static_cast<std::size_t>(data->TotalIdxCount));
    std::size_t command_count = 0;
    for (const auto* source : data->CmdLists) command_count += source->CmdBuffer.size();
    list.commands_.reserve(command_count);
    for (const auto* source : data->CmdLists) {
        const auto base_vertex = static_cast<std::uint32_t>(list.vertices_.size());
        const auto base_index = static_cast<std::uint32_t>(list.indices_.size());
        for (const auto& vertex : source->VtxBuffer)
            list.vertices_.push_back({(vertex.pos.x - data->DisplayPos.x) * scale.x,
                                      (vertex.pos.y - data->DisplayPos.y) * scale.y, vertex.uv.x,
                                      vertex.uv.y, vertex.col});
        for (const auto index : source->IdxBuffer) list.indices_.push_back(base_vertex + index);
        for (const auto& command : source->CmdBuffer) {
            if (command.UserCallback) {
                if (command.UserCallback == ImDrawCallback_ResetRenderState) continue;
                throw std::invalid_argument("ui.unsupported_draw_callback");
            }
            const auto found = textures.find(command.GetTexID());
            if (found == textures.end()) throw std::invalid_argument("ui.unknown_texture");
            const auto start = base_index + command.IdxOffset;
            for (std::uint32_t i = start; i < start + command.ElemCount; ++i)
                list.indices_.at(i) += command.VtxOffset;
            list.commands_.push_back({found->second,
                                      start,
                                      command.ElemCount,
                                      {(command.ClipRect.x - data->DisplayPos.x) * scale.x,
                                       (command.ClipRect.y - data->DisplayPos.y) * scale.y,
                                       (command.ClipRect.z - command.ClipRect.x) * scale.x,
                                       (command.ClipRect.w - command.ClipRect.y) * scale.y}});
        }
    }
    return list;
}
}  // namespace rhythm::platform
