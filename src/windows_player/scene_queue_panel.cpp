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
        static constexpr std::array kStates{
                "scene.waiting",       "scene.loading", "scene.cpu_ready",    "scene.failed",
                "scene.gpu_preparing", "scene.ready",   "scene.transitioning"};
        for (const auto& item : items) {
            auto title = item.title_ + " | " +
                         text.at(kStates.at(static_cast<std::size_t>(item.state_))) + " ";
            if (item.entry_) {
                static constexpr std::array resolutions{
                        "performance.exact", "performance.updated", "performance.missing",
                        "performance.changed", "performance.ambiguous"};
                title += text.at(resolutions.at(static_cast<std::size_t>(item.resolution_)));
            }
            title += "###scene.row." + std::to_string(item.id_);
            if (ImGui::Selectable(title.c_str(), selected_ == item.id_)) selected_ = item.id_;
        }
    }
    ImGui::EndChild();
    for (const auto& item : items)
        if (item.id_ == selected_ && !item.preparation_error_.empty())
            ImGui::TextWrapped("%s", item.preparation_error_.c_str());
    for (const auto& item : items)
        if (item.id_ == selected_ && !item.resolution_error_.empty()) {
            const auto translated = text.find(item.resolution_error_);
            ImGui::TextWrapped("%s", translated == text.end() ? item.resolution_error_.c_str()
                                                              : translated->second.c_str());
        }
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
    const auto next_entry = queue.Items().empty() ? std::optional<performance::ListEntry>{}
                                                  : queue.Items().front().entry_;
    ImGui::BeginDisabled(next_entry.has_value());
    ImGui::SetNextItemWidth(180);
    ImGui::SliderFloat(label("scene.duration").c_str(), &duration_, 0, 5, "%.2f s",
                       ImGuiSliderFlags_AlwaysClamp);
    ImGui::EndDisabled();
    if (next_entry) {
        static constexpr std::array modes{"beat.immediate", "beat.next_beat", "beat.next_bar"};
        ImGui::Text("%s: %.2f s | %s", text.at("performance.entry_settings").c_str(),
                    next_entry->transition_seconds_,
                    text.at(modes.at(static_cast<std::size_t>(next_entry->quantization_))).c_str());
    }
    const bool can_go = !queue.Items().empty() && deck.QueueReady(queue.Items().front().id_) &&
                        queue.Items().front().state_ == player::ScenePreparation::kPresentable;
    ImGui::BeginDisabled(!can_go);
    // Keyboard/navigation activation may have been queued on a previous frame.
    // Recheck domain availability even when the current button is disabled.
    if (ImGui::Button(label("scene.go").c_str()) && can_go) {
        const auto& item = queue.Items().front();
        deck.RequestNextScene(item.id_, item.entry_ ? item.entry_->transition_seconds_ : duration_,
                              item.entry_ ? item.entry_->quantization_ : mode);
    }
    ImGui::EndDisabled();
    if (!queue.Items().empty() && deck.CanHardCut(queue.Items().front().id_)) {
        ImGui::SameLine();
        if (ImGui::Button(label("scene.gpu_hard_cut").c_str()))
            deck.RequestHardCut(queue.Items().front().id_);
        ImGui::TextWrapped("%s", text.at("scene.gpu_hard_cut_hint").c_str());
    }
    if (deck.RestoringGraphics()) {
        ImGui::TextUnformatted(text.at("scene.gpu_recover").c_str());
        if (deck.GraphicsPreparation().state_ == runtime::PreparationState::kFailed &&
            ImGui::Button(label("scene.gpu_retry_recovery").c_str()))
            deck.RetryGraphicsRecovery();
    } else if (deck.Transitioning() || deck.PreparingGraphics()) {
        ImGui::SameLine();
        if (ImGui::Button(label("scene.cancel").c_str())) deck.CancelTransition();
        const auto& preparation = deck.GraphicsPreparation();
        const auto progress =
                deck.PreparingGraphics()
                        ? (preparation.total_nodes_
                                   ? double(preparation.completed_nodes_) / preparation.total_nodes_
                                   : 0)
                        : deck.Progress();
        ImGui::ProgressBar(static_cast<float>(progress), {-1, 0}, deck.IncomingTitle().c_str());
        if (deck.PreparingGraphics())
            ImGui::TextUnformatted(text.at("scene.gpu_preparing").c_str());
        else if (deck.AudioPendingId() && deck.Progress() == 0)
            ImGui::TextUnformatted(text.at("scene.audio_wait").c_str());
    } else if (deck.AudioPendingId())
        ImGui::TextUnformatted(text.at("scene.audio_recover").c_str());
    if (deck.Error() != player::SceneTransitionError::kNone)
        ImGui::TextWrapped(
                "%s", text.at(deck.Error() == player::SceneTransitionError::kBudget ? "scene.budget"
                              : deck.Error() == player::SceneTransitionError::kAudio
                                      ? "scene.audio_failed"
                                      : "scene.transition_failed")
                              .c_str());
    if (!deck.ErrorDetail().empty()) ImGui::TextWrapped("%s", deck.ErrorDetail().c_str());
    if (deck.ErrorDetail() == "audio.transition_cursor_budget")
        ImGui::TextWrapped("%s", text.at("scene.audio_hard_cut_hint").c_str());
    ImGui::End();
}
}  // namespace rhythm::player_ui
