#pragma once

#include <array>

#include "rhythm/graph/registry.h"

namespace rhythm::studio {
class ComponentInterface final {
   public:
    std::optional<graph::ComponentDefinition> Draw(const graph::ComponentDefinition& definition,
                                                   const graph::Document& body,
                                                   graph::NodeId selected,
                                                   const graph::Registry& registry,
                                                   const std::map<std::string, std::string>& text);

   private:
    std::array<char, 129> name_{};
    std::array<char, 129> group_{};
};
}  // namespace rhythm::studio
