#include "performance_panel.h"

#include <imgui.h>

#include <algorithm>

namespace rhythm::player_ui {
void PerformancePanel::Draw(player::PerformanceProgram& program,
                            std::span<const SceneChoice> choices, const std::string& locale,
                            const std::map<std::string, std::string>& text) {
    const auto label = [&](const std::string& key) { return text.at(key) + "###" + key; };
    if (ImGui::Button(label("performance.open").c_str())) {
        visible_ = true;
        ImGui::SetNextWindowFocus();
    }
    if (!visible_) return;
    ImGui::SetNextWindowSize({690, 600}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(label("performance.open").c_str(), &visible_)) {
        ImGui::End();
        return;
    }
    ImGui::TextWrapped("%s", text.at("performance.help").c_str());
    const auto& status = program.Status();
    ImGui::TextUnformatted(text.at(status.dirty_   ? "performance.unsaved"
                                   : status.saved_ ? "performance.saved"
                                                   : "performance.new")
                                   .c_str());
    if (!status.error_.empty()) {
        ImGui::TextWrapped("%s", text.at("performance.failed").c_str());
        ImGui::TextWrapped("%s", status.error_.c_str());
    }
    ImGui::BeginDisabled(program.Busy());
    if (ImGui::Button(label("performance.save").c_str())) program.Save();
    ImGui::SameLine();
    if (ImGui::Button(label("performance.reopen").c_str())) program.Load();
    ImGui::SameLine();
    if (ImGui::Button(label("performance.prepare").c_str())) program.Resolve();
    if (!choices.empty()) {
        choice_ = std::min(choice_, choices.size() - 1);
        if (ImGui::BeginCombo(label("performance.builtin").c_str(),
                              choices[choice_].titles_.at(locale).c_str())) {
            for (std::size_t index = 0; index < choices.size(); ++index)
                if (ImGui::Selectable(choices[index].titles_.at(locale).c_str(), choice_ == index))
                    choice_ = index;
            ImGui::EndCombo();
        }
        ImGui::BeginDisabled(program.Draft().Entries().size() == performance::List::kMaximumItems);
        if (ImGui::Button(label("performance.add").c_str())) {
            auto list = program.Draft();
            if (const auto id = list.Append(
                        {0, choices[choice_].reference_, choices[choice_].titles_.at(locale),
                         duration_, static_cast<parameters::Quantization>(quantization_)})) {
                if (program.Edit(std::move(list))) selected_ = *id;
            }
        }
        ImGui::EndDisabled();
    }
    const auto entries = program.Draft().Entries();
    auto selected = std::find_if(entries.begin(), entries.end(),
                                 [&](const auto& entry) { return entry.id_ == selected_; });
    if (selected == entries.end() && !entries.empty()) {
        selected_ = entries.front().id_;
        selected = entries.begin();
    }
    if (ImGui::BeginChild("###performance.rows", {0, 145}, ImGuiChildFlags_Borders)) {
        for (const auto& entry : entries) {
            const auto row = entry.title_ + "###performance.row." + std::to_string(entry.id_);
            if (ImGui::Selectable(row.c_str(), selected_ == entry.id_)) selected_ = entry.id_;
        }
    }
    ImGui::EndChild();
    selected = std::find_if(entries.begin(), entries.end(),
                            [&](const auto& entry) { return entry.id_ == selected_; });
    const auto position =
            selected == entries.end() ? 0 : static_cast<std::size_t>(selected - entries.begin());
    // Gather gestures before committing so no borrowed entry/span survives a mutation.
    ImGui::BeginDisabled(selected == entries.end());
    ImGui::BeginDisabled(position == 0);
    const bool up = ImGui::Button(label("performance.up").c_str());
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(position + 1 >= entries.size());
    const bool down = ImGui::Button(label("performance.down").c_str());
    ImGui::EndDisabled();
    ImGui::SameLine();
    const bool remove = ImGui::Button(label("performance.remove").c_str());
    ImGui::SameLine();
    ImGui::BeginDisabled(entries.size() == performance::List::kMaximumItems);
    const bool duplicate = ImGui::Button(label("performance.duplicate").c_str());
    ImGui::EndDisabled();
    ImGui::EndDisabled();
    ImGui::SliderFloat(label("performance.duration").c_str(), &duration_, 0, 5, "%.2f s",
                       ImGuiSliderFlags_AlwaysClamp);
    const std::array modes{text.at("beat.immediate"), text.at("beat.next_beat"),
                           text.at("beat.next_bar")};
    if (ImGui::BeginCombo(label("performance.quantization").c_str(),
                          modes.at(static_cast<std::size_t>(quantization_)).c_str())) {
        for (int index = 0; index < 3; ++index)
            if (ImGui::Selectable(modes[static_cast<std::size_t>(index)].c_str(),
                                  quantization_ == index))
                quantization_ = index;
        ImGui::EndCombo();
    }
    ImGui::BeginDisabled(selected == entries.end());
    const bool apply = ImGui::Button(label("performance.apply").c_str());
    if (selected != entries.end()) {
        ImGui::SameLine();
        ImGui::Text("%.2f s | %s", selected->transition_seconds_,
                    modes.at(static_cast<std::size_t>(selected->quantization_)).c_str());
        bool follow = selected->work_.policy_ == performance::VersionPolicy::kCurrentBuiltin;
        const bool policy_changed =
                selected->work_.source_ == performance::WorkSource::kBuiltin &&
                ImGui::Checkbox(label("performance.follow_builtin").c_str(), &follow);
        if (up || down || remove || duplicate || apply || policy_changed) {
            auto entry = *selected;
            auto list = program.Draft();
            if (up && position > 0) list.Move(selected_, position - 1);
            if (down && position + 1 < entries.size()) list.Move(selected_, position + 1);
            if (remove) list.Remove(selected_);
            if (duplicate) {
                if (const auto id = list.Append(entry)) selected_ = *id;
            }
            if (apply) {
                entry.transition_seconds_ = duration_;
                entry.quantization_ = static_cast<parameters::Quantization>(quantization_);
            }
            if (policy_changed)
                entry.work_.policy_ = follow ? performance::VersionPolicy::kCurrentBuiltin
                                             : performance::VersionPolicy::kExact;
            if (apply || policy_changed) {
                list.Replace(std::move(entry));
            }
            program.Edit(std::move(list));
        }
    }
    ImGui::EndDisabled();
    ImGui::Separator();
    ImGui::InputText(label("performance.import_path").c_str(), import_path_.data(),
                     import_path_.size());
    ImGui::BeginDisabled(!import_path_[0] ||
                         program.Draft().Entries().size() == performance::List::kMaximumItems);
    if (ImGui::Button(label("performance.import").c_str())) {
        const std::string utf8(import_path_.data());
        program.Import(std::filesystem::path(std::u8string(utf8.begin(), utf8.end())), duration_,
                       static_cast<parameters::Quantization>(quantization_));
    }
    ImGui::EndDisabled();
    ImGui::EndDisabled();
    if (program.Busy()) {
        ImGui::TextUnformatted(text.at("performance.busy").c_str());
        ImGui::SameLine();
        if (ImGui::Button(label("performance.cancel").c_str())) program.Cancel();
    }
    ImGui::End();
}
}  // namespace rhythm::player_ui
