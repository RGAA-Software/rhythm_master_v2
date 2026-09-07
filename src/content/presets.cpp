#include "rhythm/content/presets.h"

#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>
#include <set>
#include <stdexcept>

namespace rhythm::content {
namespace {
constexpr std::size_t kMaximumCatalogBytes = 1024 * 1024;
bool ValidId(const std::string& id) {
    return !id.empty() && id.size() <= 128 && std::all_of(id.begin(), id.end(), [](char value) {
        return (value >= 'a' && value <= 'z') || (value >= '0' && value <= '9') || value == '.' ||
               value == '-' || value == '_';
    });
}
}  // namespace
graph::Node ApplyPreset(const graph::Node& node, const Preset& preset,
                        const graph::Registry& registry,
                        std::span<const graph::ComponentDefinition> components) {
    if (node.type_ != preset.operator_type_)
        throw std::invalid_argument("content.operator_mismatch");
    auto resolved = node;
    if (preset.reset_)
        resolved.properties_ = registry.MakeNode(node.id_, node.type_, components).properties_;
    for (const auto& [key, value] : preset.properties_) resolved.properties_[key] = value;
    if (!registry.ValidateNode(resolved, components).empty())
        throw std::invalid_argument("content.properties");
    return resolved;
}
std::vector<Preset> DecodePresets(std::string_view bytes, const graph::Registry& registry,
                                  std::span<const graph::ComponentDefinition> components) {
    if (bytes.size() > kMaximumCatalogBytes) throw std::length_error("content.catalog_bytes");
    using Json = nlohmann::json;
    std::vector<std::set<std::string>> keys;
    const auto catalog = Json::parse(bytes, [&](int depth, Json::parse_event_t event, Json& value) {
        if (depth > 16) throw std::length_error("content.depth");
        if (event == Json::parse_event_t::object_start) keys.emplace_back();
        if (event == Json::parse_event_t::key &&
            !keys.back().insert(value.get<std::string>()).second)
            throw std::invalid_argument("content.duplicate_key");
        if (event == Json::parse_event_t::object_end) keys.pop_back();
        return true;
    });
    if (catalog.at("schema_version") != 1 || !catalog.at("presets").is_array() ||
        catalog.at("presets").size() > 1024)
        throw std::invalid_argument("content.schema");
    std::vector<Preset> presets;
    std::set<std::string> ids;
    for (const auto& record : catalog.at("presets")) {
        Preset preset;
        preset.reset_ = record.value("reset", false);
        preset.id_ = record.at("id").get<std::string>();
        preset.version_ = record.at("version").get<std::string>();
        preset.operator_type_ = record.at("operator").get<std::string>();
        preset.titles_ = record.at("titles").get<std::map<std::string, std::string>>();
        if (!ValidId(preset.id_) || !ids.insert(preset.id_).second || preset.version_ != "1.0.0")
            throw std::invalid_argument("content.identity");
        for (const auto& locale : {"zh-CN", "en-US"})
            if (!preset.titles_.contains(locale) || preset.titles_.at(locale).empty() ||
                preset.titles_.at(locale).size() > 512)
                throw std::invalid_argument("content.locale");
        const auto& properties = record.at("properties");
        if (!properties.is_object() || properties.size() > 128)
            throw std::length_error("content.property_count");
        for (const auto& [key, value] : properties.items()) {
            if (value.is_number())
                preset.properties_[key] = value.get<double>();
            else if (value.is_object() && value.size() == 1 && value.contains("asset_sha256") &&
                     value.at("asset_sha256").is_string())
                preset.properties_[key] =
                        assets::AssetId{value.at("asset_sha256").get<std::string>()};
            else if (value.is_object() && value.size() == 1 && value.contains("expression") &&
                     value.at("expression").is_string())
                preset.properties_[key] =
                        parameters::Expression(value.at("expression").get<std::string>());
            else if (value.is_array() && value.size() == 4) {
                for (const auto& channel : value)
                    if (!channel.is_number()) throw std::invalid_argument("content.color");
                preset.properties_[key] =
                        graph::Color{value[0].get<double>(), value[1].get<double>(),
                                     value[2].get<double>(), value[3].get<double>()};
            } else if (value.is_object() && value.contains("curve") &&
                       value.at("curve").is_array()) {
                if (value.at("curve").size() > parameters::Curve::kMaximumKeys)
                    throw std::length_error("curve.key_count");
                std::vector<parameters::Keyframe> curve_keys;
                for (const auto& keyframe : value.at("curve")) {
                    const auto mode = keyframe.at("interpolation").get<std::string>();
                    if (mode != "step" && mode != "linear" && mode != "smooth")
                        throw std::invalid_argument("curve.interpolation");
                    curve_keys.push_back({keyframe.at("seconds").get<double>(),
                                          keyframe.at("value").get<double>(),
                                          mode == "step"     ? parameters::Interpolation::kStep
                                          : mode == "smooth" ? parameters::Interpolation::kSmooth
                                                             : parameters::Interpolation::kLinear});
                }
                preset.properties_[key] = parameters::Curve(std::move(curve_keys));
            } else
                throw std::invalid_argument("content.property_type");
        }
        ApplyPreset(registry.MakeNode(1, preset.operator_type_, components), preset, registry,
                    components);
        presets.push_back(std::move(preset));
    }
    return presets;
}
std::vector<Preset> LoadPresets(const std::filesystem::path& path, const graph::Registry& registry,
                                std::span<const graph::ComponentDefinition> components) {
    const auto size = std::filesystem::file_size(path);
    if (size > kMaximumCatalogBytes) throw std::length_error("content.catalog_bytes");
    std::ifstream file(path, std::ios::binary);
    file.exceptions(std::ios::failbit | std::ios::badbit);
    std::string bytes(static_cast<std::size_t>(size), '\0');
    file.read(bytes.data(), static_cast<std::streamsize>(size));
    return DecodePresets(bytes, registry, components);
}
}  // namespace rhythm::content
