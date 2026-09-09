#include "waveform_panel.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace rhythm::studio {
std::optional<double> DrawWaveform(const media::WaveformOverview& overview, double seconds) {
    if (!overview.frames_ || !overview.frames_per_peak_ || !overview.count_ ||
        overview.count_ > media::kMaximumWaveformBins)
        return {};
    const auto start = ImGui::GetCursorScreenPos();
    const float width = std::max(1.0F, ImGui::GetContentRegionAvail().x);
    const float height = 104;
    const bool seek = ImGui::InvisibleButton("###waveform.seek", {width, height});
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    auto& draw = *ImGui::GetWindowDrawList();
    draw.AddRectFilled(start, {start.x + width, start.y + height},
                       ImGui::GetColorU32(ImGuiCol_FrameBg), 4);
    const float center = start.y + 40;
    const auto frames = static_cast<double>(overview.frames_);
    const int columns = std::max(1, std::min(4096, static_cast<int>(width)));
    for (int column = 0; column < columns; ++column) {
        const auto begin = static_cast<std::size_t>(frames * column / columns /
                                                    static_cast<double>(overview.frames_per_peak_));
        const auto end =
                std::min(overview.count_, static_cast<std::size_t>(std::ceil(
                                                  frames * (column + 1) / columns /
                                                  static_cast<double>(overview.frames_per_peak_))));
        float minimum = 0, maximum = 0;
        for (auto index = begin; index < end; ++index) {
            minimum = std::min(minimum, overview.peaks_[index].minimum_);
            maximum = std::max(maximum, overview.peaks_[index].maximum_);
        }
        const float x = start.x + width * (static_cast<float>(column) + 0.5F) / columns;
        draw.AddLine({x, center - maximum * 36}, {x, center - minimum * 36},
                     ImGui::GetColorU32(ImGuiCol_PlotHistogram));
    }
    for (int index = 0; index <= 4; ++index) {
        const float x = start.x + width * index / 4;
        draw.AddLine({x, start.y}, {x, start.y + 80}, ImGui::GetColorU32(ImGuiCol_Border));
        char label[32]{};
        std::snprintf(label, sizeof(label), "%.1f s", overview.Duration() * index / 4);
        const auto size = ImGui::CalcTextSize(label);
        draw.AddText({std::clamp(x - size.x / 2, start.x, start.x + std::max(0.0F, width - size.x)),
                      start.y + 84},
                     ImGui::GetColorU32(ImGuiCol_TextDisabled), label);
    }
    const auto fraction = std::clamp(
            static_cast<double>((ImGui::GetIO().MousePos.x - start.x) / width), 0.0, 1.0);
    const auto position =
            active ? fraction
                   : std::clamp(std::isfinite(seconds) ? seconds / overview.Duration() : 0.0, 0.0,
                                1.0);
    const float marker = start.x + static_cast<float>(position) * width;
    draw.AddLine({marker, start.y}, {marker, start.y + 80},
                 ImGui::GetColorU32(ImGuiCol_SliderGrabActive), 2);
    if (hovered) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        ImGui::SetTooltip("%.3f s", fraction * overview.Duration());
    }
    return seek ? std::optional(fraction * overview.Duration()) : std::nullopt;
}
void WaveformPanel::Clear() {
    if (scanner_) {
        scanner_->Cancel();
        if (scanner_->Take()) scanner_.reset();
    }
    clips_.Clear();
    source_.reset();
    overview_.reset();
    error_.clear();
    needs_scan_ = false;
    ++generation_;
}
std::optional<double> WaveformPanel::Draw(const std::optional<std::filesystem::path>& source,
                                          double seconds,
                                          const std::map<std::string, std::string>& text) {
    clips_.Clear();
    if (clips_.Busy()) return {};
    if (source != source_) {
        Clear();
        source_ = source;
        needs_scan_ = source.has_value();
    }
    if (scanner_)
        if (auto result = scanner_->Take()) {
            if (active_generation_ == generation_) {
                overview_ = std::move(result->overview_);
                error_ = std::move(result->error_);
            }
            scanner_.reset();
        }
    if (!source_) return {};
    if (needs_scan_ && (!scanner_ || !scanner_->Busy())) {
        needs_scan_ = false;
        try {
            if (!scanner_) scanner_.emplace();
            active_generation_ = generation_;
            if (!scanner_->Start(*source_)) error_ = "waveform.failed";
        } catch (const std::exception&) {
            error_ = "waveform.failed";
        }
    }
    ImGui::TextUnformatted(text.at("waveform.title").c_str());
    ImGui::SameLine();
    if (ImGui::SmallButton((text.at("waveform.refresh") + "###waveform.refresh").c_str())) {
        Clear();
        source_ = source;
        needs_scan_ = true;
    }
    if (scanner_ && scanner_->Busy()) {
        ImGui::SameLine();
        if (ImGui::SmallButton((text.at("asset.cancel") + "###waveform.cancel").c_str())) {
            scanner_->Cancel();
            needs_scan_ = false;
        }
        const auto progress = active_generation_ == generation_ ? scanner_->SecondsScanned() : 0;
        ImGui::Text("%s %.1f s", text.at("waveform.scanning").c_str(), progress);
    }
    if (!error_.empty()) ImGui::TextWrapped("%s", text.at(error_).c_str());
    if (!overview_) return {};
    ImGui::TextWrapped("%s", text.at("waveform.help").c_str());
    return DrawWaveform(*overview_, seconds);
}
ClipWaveforms WaveformPanel::DrawClips(const std::filesystem::path& directory,
                                       std::span<const media::AudioClip> clips,
                                       std::span<const assets::AssetRecord> assets,
                                       const std::map<std::string, std::string>& text) {
    if (source_ || overview_ || scanner_) Clear();
    if (scanner_) return {};
    std::vector<rhythm::assets::AssetRecord> records;
    bool missing = false;
    for (const auto& clip : clips) {
        if (std::any_of(records.begin(), records.end(),
                        [&](const auto& record) { return record.id_ == clip.asset_; }))
            continue;
        const auto found = std::find_if(assets.begin(), assets.end(), [&](const auto& record) {
            return record.id_ == clip.asset_;
        });
        if (found == assets.end())
            missing = true;
        else
            records.push_back(*found);
    }
    clips_.Update(directory, records);
    ImGui::Text("%s %zu / %zu", text.at("waveform.clips").c_str(), clips_.ReadyCount(),
                records.size());
    if (clips_.Busy()) {
        ImGui::SameLine();
        ImGui::TextUnformatted(text.at("waveform.scanning").c_str());
    }
    if (missing || clips_.FailureCount()) {
        ImGui::TextWrapped("%s", text.at("waveform.failed").c_str());
        if (ImGui::SmallButton((text.at("waveform.refresh") + "###waveform.clips_retry").c_str()))
            clips_.Retry();
    }
    ClipWaveforms result;
    for (const auto& record : records)
        if (auto waveform = clips_.Find(record.id_))
            result.emplace(record.id_.sha256_, std::move(waveform));
    return result;
}
}  // namespace rhythm::studio
