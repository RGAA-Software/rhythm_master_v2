#include "control_codec.h"

#include <stdexcept>

namespace rhythm::project::detail {
void DecodeControls(const schema::ControlMetadata& record, graph::Document& document) {
    if (record.titles_size() > 64 || record.snapshots_size() > 64)
        throw std::length_error("control.budget");
    for (const auto& [id, title] : record.titles()) document.control_titles_.emplace(id, title);
    for (const auto& snapshot : record.snapshots()) {
        if (snapshot.values_size() > 64) throw std::length_error("control.budget");
        parameters::ControlSnapshot value{snapshot.id(), snapshot.title()};
        for (const auto& [id, scalar] : snapshot.values()) value.values_.emplace(id, scalar);
        document.control_snapshots_.push_back(std::move(value));
    }
}
void EncodeControls(const graph::Document& document, schema::ControlMetadata& record) {
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
