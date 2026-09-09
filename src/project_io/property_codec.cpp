#include "property_codec.h"

#include <stdexcept>

namespace rhythm::project::detail {
graph::Property DecodeProperty(const schema::Property& property, bool preserve_unknown) {
    if (property.has_event_track()) {
        const auto& encoded = property.event_track();
        if (encoded.actions_size() > static_cast<int>(parameters::EventTrack::kMaximumEvents))
            throw std::length_error("event.track_count");
        std::vector<parameters::RecordedEvent> actions;
        actions.reserve(encoded.actions_size());
        for (const auto& action : encoded.actions()) {
            if (action.kind() < schema::EventTrack::KIND_PULSE ||
                action.kind() > schema::EventTrack::KIND_RESET)
                throw std::invalid_argument("event.track_kind");
            actions.push_back({action.id(), action.seconds(),
                               static_cast<parameters::EventKind>(action.kind() - 1),
                               action.value()});
        }
        return parameters::EventTrack(std::move(actions));
    }
    if (property.has_scalar()) return property.scalar();
    if (property.has_expression()) return parameters::Expression(property.expression());
    if (property.has_asset_sha256()) {
        assets::AssetId asset{property.asset_sha256()};
        if (!asset.sha256_.empty() && !assets::ValidId(asset))
            throw std::invalid_argument("project.asset_reference");
        return asset;
    }
    if (property.has_color()) {
        const auto& color = property.color();
        return graph::Color{color.r(), color.g(), color.b(), color.a()};
    }
    if (property.has_curve()) {
        const auto& encoded = property.curve();
        if (encoded.keys_size() > static_cast<int>(parameters::Curve::kMaximumKeys))
            throw std::length_error("curve.key_count");
        std::vector<parameters::Keyframe> keys;
        for (const auto& key : encoded.keys()) {
            if (key.interpolation() < schema::Curve::INTERPOLATION_STEP ||
                key.interpolation() > schema::Curve::INTERPOLATION_HERMITE)
                throw std::invalid_argument("curve.interpolation");
            keys.push_back({key.seconds(), key.value(),
                            static_cast<parameters::Interpolation>(key.interpolation() - 1),
                            key.in_slope(), key.out_slope()});
        }
        return parameters::Curve(std::move(keys));
    }
    if (preserve_unknown) return graph::UnknownProperty{property.SerializeAsString()};
    throw std::invalid_argument("package.property");
}
void EncodeProperty(const graph::Property& value, schema::Property& property) {
    if (std::holds_alternative<double>(value))
        property.set_scalar(std::get<double>(value));
    else if (std::holds_alternative<parameters::Expression>(value))
        property.set_expression(std::string(std::get<parameters::Expression>(value).Source()));
    else if (std::holds_alternative<assets::AssetId>(value)) {
        const auto& asset = std::get<assets::AssetId>(value);
        if (!asset.sha256_.empty() && !assets::ValidId(asset))
            throw std::invalid_argument("project.asset_reference");
        property.set_asset_sha256(asset.sha256_);
    } else if (std::holds_alternative<graph::Color>(value)) {
        const auto color = std::get<graph::Color>(value);
        auto& encoded = *property.mutable_color();
        encoded.set_r(color.r_);
        encoded.set_g(color.g_);
        encoded.set_b(color.b_);
        encoded.set_a(color.a_);
    } else if (std::holds_alternative<parameters::Curve>(value)) {
        auto& curve = *property.mutable_curve();
        const auto original = curve.keys();
        curve.clear_keys();
        for (const auto& key : std::get<parameters::Curve>(value).Keys()) {
            auto& encoded = *curve.add_keys();
            // Preserve extension fields for an existing key at the same time.
            for (const auto& previous : original)
                if (previous.seconds() == key.seconds_) {
                    encoded = previous;
                    break;
                }
            encoded.set_seconds(key.seconds_);
            encoded.set_value(key.value_);
            encoded.set_interpolation(static_cast<schema::Curve::Interpolation>(
                    static_cast<int>(key.interpolation_) + 1));
            encoded.set_in_slope(key.in_slope_);
            encoded.set_out_slope(key.out_slope_);
        }
    } else if (std::holds_alternative<parameters::EventTrack>(value)) {
        auto& track = *property.mutable_event_track();
        const auto original = track.actions();
        std::map<std::uint64_t, int> indices;
        for (int index = 0; index < original.size(); ++index)
            indices.emplace(original.Get(index).id(), index);
        track.clear_actions();
        for (const auto& action : std::get<parameters::EventTrack>(value).Events()) {
            auto& encoded = *track.add_actions();
            if (const auto previous = indices.find(action.id_); previous != indices.end())
                encoded = original.Get(previous->second);
            encoded.set_id(action.id_);
            encoded.set_seconds(action.seconds_);
            encoded.set_kind(
                    static_cast<schema::EventTrack::Kind>(static_cast<int>(action.kind_) + 1));
            encoded.set_value(action.value_);
        }
    } else if (!property.ParseFromString(std::get<graph::UnknownProperty>(value).encoded_))
        throw std::invalid_argument("project.extensions");
}
}  // namespace rhythm::project::detail
