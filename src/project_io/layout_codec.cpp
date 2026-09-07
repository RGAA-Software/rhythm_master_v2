#include "layout_codec.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace rhythm::project {
namespace {
using Json = nlohmann::json;
Json Positions(const std::map<graph::NodeId, editor::Position>& positions) {
    Json result = Json::array();
    for (const auto& [id, position] : positions) {
        if (!std::isfinite(position.x_) || !std::isfinite(position.y_))
            throw std::invalid_argument("project.layout");
        result.push_back({{"id", id}, {"x", position.x_}, {"y", position.y_}});
    }
    return result;
}
std::map<graph::NodeId, editor::Position> ReadPositions(const Json& positions,
                                                        std::span<const graph::Node> nodes,
                                                        std::size_t& total) {
    total += positions.size();
    if (!positions.is_array() || total > 10000) throw std::invalid_argument("project.layout");
    std::map<graph::NodeId, editor::Position> result;
    for (const auto& item : positions) {
        if (!item.at("id").is_number_unsigned()) throw std::invalid_argument("project.layout");
        const auto id = item.at("id").get<graph::NodeId>();
        const editor::Position position{item.at("x").get<float>(), item.at("y").get<float>()};
        if (!std::isfinite(position.x_) || !std::isfinite(position.y_) ||
            std::none_of(nodes.begin(), nodes.end(),
                         [&](const auto& node) { return node.id_ == id; }) ||
            !result.emplace(id, position).second)
            throw std::invalid_argument("project.layout");
    }
    return result;
}
}  // namespace
Json EncodeLayout(const editor::Snapshot& snapshot) {
    if (snapshot.component_positions_.size() > 256 || snapshot.positions_.size() > 10000)
        throw std::invalid_argument("project.layout");
    Json result{{"version", snapshot.component_positions_.empty() ? 1 : 2},
                {"positions", Positions(snapshot.positions_)}};
    if (!snapshot.component_positions_.empty()) {
        result["components"] = Json::array();
        std::size_t total = snapshot.positions_.size();
        for (const auto& [type, positions] : snapshot.component_positions_) {
            total += positions.size();
            if (total > 10000 || type.size() > 256) throw std::invalid_argument("project.layout");
            result["components"].push_back({{"type", type}, {"positions", Positions(positions)}});
        }
    }
    auto checked = snapshot;
    DecodeLayout(result, checked);
    return result;
}
void DecodeLayout(const Json& layout, editor::Snapshot& snapshot) {
    const auto version = layout.at("version");
    if (version != 1 && version != 2) throw std::invalid_argument("project.layout");
    std::size_t total = 0;
    auto positions = ReadPositions(layout.at("positions"), snapshot.document_.nodes_, total);
    decltype(snapshot.component_positions_) components;
    if (layout.contains("components")) {
        const auto& records = layout.at("components");
        if (version != 2 || !records.is_array() || records.size() > 256)
            throw std::invalid_argument("project.layout");
        for (const auto& record : records) {
            const auto type = record.at("type").get<std::string>();
            const auto& library = snapshot.document_.components_;
            const auto definition =
                    std::find_if(library.begin(), library.end(),
                                 [&](const auto& value) { return value.type_ == type; });
            if (type.size() > 256 || definition == library.end() || components.contains(type))
                throw std::invalid_argument("project.layout");
            components.emplace(type,
                               ReadPositions(record.at("positions"), definition->nodes_, total));
        }
    }
    snapshot.positions_ = std::move(positions);
    snapshot.component_positions_ = std::move(components);
}
}  // namespace rhythm::project
