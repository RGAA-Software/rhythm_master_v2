#include "control_codec.h"

#include <stdexcept>

namespace rhythm::project::detail {
parameters::BeatSettings DecodeBeatGrid(const schema::BeatGrid& record) {
    const parameters::BeatSettings settings{record.bpm(), record.beats_per_bar(),
                                            record.beat_unit(), record.origin_seconds()};
    if (!parameters::ValidBeatSettings(settings))
        throw std::invalid_argument("project.beat_settings");
    return settings;
}
void EncodeBeatGrid(const parameters::BeatSettings& settings, schema::BeatGrid& record) {
    if (!parameters::ValidBeatSettings(settings))
        throw std::invalid_argument("project.beat_settings");
    record.set_bpm(settings.bpm_);
    record.set_beats_per_bar(settings.beats_per_bar_);
    record.set_beat_unit(settings.beat_unit_);
    record.set_origin_seconds(settings.origin_seconds_);
}
void DecodeControls(const schema::ControlMetadata& record, graph::Document& document) {
    if (record.titles_size() > 64 || record.snapshots_size() > 64 || record.cues_size() > 256)
        throw std::length_error("control.budget");
    for (const auto& [id, title] : record.titles()) document.control_titles_.emplace(id, title);
    for (const auto& snapshot : record.snapshots()) {
        if (snapshot.values_size() > 64) throw std::length_error("control.budget");
        parameters::ControlSnapshot value{snapshot.id(), snapshot.title()};
        for (const auto& [id, scalar] : snapshot.values()) value.values_.emplace(id, scalar);
        document.control_snapshots_.push_back(std::move(value));
    }
    for (const auto& cue : record.cues())
        document.control_cues_.push_back(
                {cue.id(), cue.title(), cue.seconds(), cue.snapshot(), cue.fade(), cue.smooth()});
}
void EncodeControls(const graph::Document& document, schema::ControlMetadata& record) {
    const auto old_cues = record.cues();
    record.clear_cues();
    for (const auto& cue : document.control_cues_) {
        auto& value = *record.add_cues();
        for (const auto& previous : old_cues)
            if (previous.id() == cue.id_) {
                value = previous;
                break;
            }
        value.set_id(cue.id_);
        value.set_title(cue.title_);
        value.set_seconds(cue.seconds_);
        value.set_snapshot(cue.snapshot_);
        value.set_fade(cue.fade_);
        value.set_smooth(cue.smooth_);
    }
    record.clear_titles();
    for (const auto& [id, title] : document.control_titles_) (*record.mutable_titles())[id] = title;
    auto original = record.snapshots();
    record.clear_snapshots();
    for (const auto& snapshot : document.control_snapshots_) {
        auto& value = *record.add_snapshots();
        for (const auto& previous : original)
            if (previous.id() == snapshot.id_) {
                value = previous;
                break;
            }
        value.set_id(snapshot.id_);
        value.set_title(snapshot.title_);
        value.clear_values();
        for (const auto& [id, scalar] : snapshot.values_) (*value.mutable_values())[id] = scalar;
    }
}
}  // namespace rhythm::project::detail
