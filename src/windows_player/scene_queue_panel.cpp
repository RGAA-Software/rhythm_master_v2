#include "scene_queue_panel.h"

#include <imgui.h>

#include <algorithm>
#include <array>

namespace rhythm::player_ui {
void SceneQueuePanel::Draw(player::SceneQueue& queue, player::SceneDeck& deck,
                           std::span<const SceneChoice> choices, const std::string& locale,
                           const std::map<std::string, std::string>& text,
                           parameters::Quantization mode) {
    const auto label = [&](const std::string& key) { return text.at(key) + "###" + key; };
    if (ImGui::Button(label("scene.queue").c_str())) {
        visible_ = true;
        ImGui::SetNextWindowFocus();
    }
    if (!visible_) return;
    ImGui::SetNextWindowSize({620, 450}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(label("scene.queue").c_str(), &visible_)) {
        ImGui::End();
        return;
    }
    ImGui::TextWrapped("%s", text.at("scene.queue_help").c_str());
    if (!choices.empty()) {
        choice_ = std::min(choice_, choices.size() - 1);
        if (ImGui::BeginCombo(label("scene.builtin").c_str(),
                              choices[choice_].titles_.at(locale).c_str())) {
            for (std::size_t index = 0; index < choices.size(); ++index)
                if (ImGui::Selectable(choices[index].titles_.at(locale).c_str(), choice_ == index))
                    choice_ = index;
            ImGui::EndCombo();
        }
        if (ImGui::Button(label("scene.enqueue").c_str())) {
            const auto id =
                    queue.Enqueue(choices[choice_].package_, choices[choice_].titles_.at(locale));
            full_ = !id;
            if (id) selected_ = *id;
        }
    }
    if (full_) ImGui::TextWrapped("%s", text.at("scene.queue_full").c_str());
    const auto items = queue.Items();
    if (!items.empty() && std::none_of(items.begin(), items.end(),
                                       [&](const auto& item) { return item.id_ == selected_; }))
        selected_ = items.front().id_;
    if (ImGui::BeginChild("###scene.rows", {0, 110}, ImGuiChildFlags_Borders)) {
        static constexpr std::array kStates{"scene.waiting", "scene.loading", "scene.ready",
                                            "scene.failed"};
        for (const auto& item : items) {
            const auto title = item.title_ + " | " +
                               text.at(kStates.at(static_cast<std::size_t>(item.state_))) +
                               "###scene.row." + std::to_string(item.id_);
            if (ImGui::Selectable(title.c_str(), selected_ == item.id_)) selected_ = item.id_;
        }
    }
    ImGui::EndChild();
    ImGui::BeginDisabled(items.empty());
    if (ImGui::Button(label("scene.remove").c_str())) queue.Remove(selected_);
    ImGui::SameLine();
    if (ImGui::Button(label("scene.clear").c_str())) {
        queue.Clear();
        full_ = false;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(queue.Items().empty() ||
                         queue.Items().front().state_ != player::ScenePreparation::kFailed);
    if (ImGui::Button(label("scene.retry").c_str())) queue.Retry();
    ImGui::EndDisabled();
    ImGui::SetNextItemWidth(180);
    ImGui::SliderFloat(label("scene.duration").c_str(), &duration_, 0, 5, "%.2f s",
                       ImGuiSliderFlags_AlwaysClamp);
    const bool can_go = deck.CanPrepareNext() && !queue.Items().empty() &&
                        queue.Items().front().state_ == player::ScenePreparation::kReady;
    ImGui::BeginDisabled(!can_go);
    // Keyboard/navigation activation may have been queued on a previous frame.
    // Recheck domain availability even when the current button is disabled.
    if (ImGui::Button(label("scene.go").c_str()) && can_go)
        deck.RequestNextScene(queue.Items().front().id_, duration_, mode);
    ImGui::EndDisabled();
    if (deck.Transitioning()) {
        ImGui::SameLine();
        if (ImGui::Button(label("scene.cancel").c_str())) deck.CancelTransition();
        ImGui::ProgressBar(static_cast<float>(deck.Progress()), {-1, 0},
                           deck.IncomingTitle().c_str());
    }
    if (deck.Error() != player::SceneTransitionError::kNone)
        ImGui::TextWrapped("%s", text.at(deck.Error() == player::SceneTransitionError::kBudget
                                                 ? "scene.budget"
                                                 : "scene.transition_failed")
                                         .c_str());
    ImGui::End();
}
}  // namespace rhythm::player_ui
