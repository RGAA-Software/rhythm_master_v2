#pragma once

#include <imgui.h>
#include <imgui_internal.h>

#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace rhythm::testing {
// Synchronous boundary for real ImGui input queues. This adapter changes UI
// input/view state only; it has no access to the authored graph or editor history.
class StudioInput final {
   public:
    explicit StudioInput(std::function<void()> frame) : frame_(std::move(frame)) {}
    void Settle(int frames = 3) const {
        for (int index = 0; index < frames; ++index) frame_();
    }
    static void Place(const std::string& name, ImVec2 position, ImVec2 size) {
        if (auto* window = ImGui::FindWindowByName(name.c_str())) {
            ImGui::SetWindowDock(window, 0, ImGuiCond_Always);
            ImGui::SetWindowPos(window, position, ImGuiCond_Always);
            ImGui::SetWindowSize(window, size, ImGuiCond_Always);
        }
    }
    void Focus(const std::string& name) const {
        auto* window = ImGui::FindWindowByName(name.c_str());
        if (!window || !window->Active) throw std::runtime_error("authoring focus window missing");
        ImGui::FocusWindow(window);
        Settle();
    }
    static std::string Popup() {
        const auto& stack = ImGui::GetCurrentContext()->OpenPopupStack;
        if (stack.empty() || !stack.back().Window || !stack.back().Window->Active)
            throw std::runtime_error("authoring popup is not active");
        return stack.back().Window->Name;
    }
    void Button(const std::string& window, const std::string& label,
                const std::string& scope = {}) const {
        Activate(window, label, scope, false);
        Settle();
    }
    void Text(const std::string& window, const std::string& label, const std::string& value) const {
        const auto id = Activate(window, label, {}, true);
        EnterText(id, value);
    }
    void Color(const std::string& property, const std::string& hex) const {
        Button("###inspector", "##ColorButton", property);
        const auto name = Popup();
        ImGuiID id = 0;
        if (const auto* window = ImGui::FindWindowByName(name.c_str())) {
            id = ImHashStr("##Text", 0,
                           ImHashStr("##hex", 0, ImHashStr("##picker", 0, window->ID)));
            ImGui::ActivateItemByID(id);
            ImGui::GetCurrentContext()->NavNextActivateFlags = ImGuiActivateFlags_PreferInput;
        }
        if (!id) throw std::runtime_error("color picker window missing");
        EnterText(id, hex);
        ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, true);
        frame_();
        ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, false);
        Settle();
    }

   private:
    void EnterText(ImGuiID id, const std::string& value) const {
        Settle();
        if (ImGui::GetCurrentContext()->ActiveId != id)
            throw std::runtime_error("authoring input not focused: " + std::to_string(id));
        auto& io = ImGui::GetIO();
        io.AddKeyEvent(ImGuiMod_Ctrl, true);
        io.AddKeyEvent(ImGuiKey_A, true);
        frame_();
        io.AddKeyEvent(ImGuiKey_A, false);
        io.AddKeyEvent(ImGuiMod_Ctrl, false);
        io.AddInputCharactersUTF8(value.c_str());
        frame_();
        io.AddKeyEvent(ImGuiKey_Enter, true);
        frame_();
        io.AddKeyEvent(ImGuiKey_Enter, false);
        Settle();
    }

   public:
    void AddNode(const std::string& type) const {
        Button("###graph", "###add", "node.palette");
        const auto popup = Popup();
        Text(popup, "###search", type);
        Button(popup, "###" + type);
    }
    void FindNode(std::uint64_t id, const std::string& type) const {
        Button("###graph", "###navigation.find");
        ImGui::GetIO().AddInputCharactersUTF8(type.c_str());
        Settle();
        std::string rows;
        for (const auto* window : ImGui::GetCurrentContext()->Windows)
            if (window->Active &&
                std::string_view(window->Name).find("navigation.rows") != std::string_view::npos)
                rows = window->Name;
        if (rows.empty()) throw std::runtime_error("authoring search rows missing");
        Button(rows, "###node." + std::to_string(id));
    }
    static ImRect ImageRect(const std::string& name) {
        const auto* window = ImGui::FindWindowByName(name.c_str());
        if (!window || !window->Active) throw std::runtime_error("authoring image window missing");
        const auto& draw = *window->DrawList;
        for (const auto& command : draw.CmdBuffer) {
            if (command.GetTexID() == ImGui::GetIO().Fonts->TexID || command.ElemCount < 6)
                continue;
            const auto first =
                    draw.VtxBuffer[draw.IdxBuffer[command.IdxOffset] + command.VtxOffset].pos;
            ImRect result(first, first);
            for (unsigned int index = 0; index < 6; ++index)
                result.Add(draw.VtxBuffer[draw.IdxBuffer[command.IdxOffset + index] +
                                          command.VtxOffset]
                                   .pos);
            return result;
        }
        throw std::runtime_error("authoring output image missing");
    }
    void Export(const std::string& name) const {
        Button("###inspector", "###binding.title");
        Text("###inspector", "###binding.name", name);
        Button("###inspector", "###binding.export");
        Button("###inspector", "###binding.title");
    }
    void Bind(const std::string& port, const std::string& source) const {
        Button("###inspector", "###binding.title");
        Button("###inspector", "###binding.input." + port);
        Button(Popup(), source);
        Button("###inspector", "###binding.title");
    }

   private:
    static ImGuiID Activate(const std::string& name, const std::string& label,
                            const std::string& scope, bool input) {
        const auto* window = ImGui::FindWindowByName(name.c_str());
        if (!window || !window->Active)
            throw std::runtime_error("authoring window missing: " + name);
        const auto seed = scope.empty() ? window->ID : ImHashStr(scope.c_str(), 0, window->ID);
        const auto id = ImHashStr(label.c_str(), 0, seed);
        ImGui::ActivateItemByID(id);
        if (input)
            ImGui::GetCurrentContext()->NavNextActivateFlags = ImGuiActivateFlags_PreferInput;
        return id;
    }
    std::function<void()> frame_{};
};
}  // namespace rhythm::testing
