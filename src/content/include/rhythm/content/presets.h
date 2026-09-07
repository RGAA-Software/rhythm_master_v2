#pragma once

#include <filesystem>

#include "rhythm/graph/registry.h"

namespace rhythm::content {
struct Preset {
    std::string id_{};
    std::string version_{};
    std::string operator_type_{};
    std::map<std::string, std::string> titles_{};
    std::map<std::string, graph::Property> properties_{};
    bool reset_ = false;
};
std::vector<Preset> DecodePresets(std::string_view bytes, const graph::Registry& registry,
                                  std::span<const graph::ComponentDefinition> components = {});
std::vector<Preset> LoadPresets(const std::filesystem::path& path, const graph::Registry& registry,
                                std::span<const graph::ComponentDefinition> components = {});
// Returns resolved values; saved projects and Player never depend on a mutable catalog.
graph::Node ApplyPreset(const graph::Node& node, const Preset& preset,
                        const graph::Registry& registry,
                        std::span<const graph::ComponentDefinition> components = {});
}  // namespace rhythm::content
