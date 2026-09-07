#pragma once

#include "rhythm/graph/registry.h"

namespace rhythm::studio {
struct TimeSectionEdit {
    std::optional<graph::Node> changed_{};
    bool committed_ = false;
    bool add_ = false;
};
// Owns selection and one bar gesture. Snapshot/history ownership stays in the
// timeline; only a changed node value crosses this interaction boundary.
class TimeSectionEditor final {
   public:
    TimeSectionEdit Draw(const graph::Document& document, double duration, bool can_add,
                         const std::map<std::string, std::string>& text);
    void Reset();

   private:
    struct Drag {
        graph::Node origin_{};
        float mouse_x_ = 0;
        int mode_ = 0;
        bool changed_ = false;
    };
    std::optional<Drag> drag_{};
    graph::NodeId selected_ = 0;
    std::string document_{};
    std::uint64_t revision_ = 0;
    bool property_active_ = false;
};
}  // namespace rhythm::studio
