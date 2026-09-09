#pragma once

#include <span>

#include "output_edit.h"
#include "rhythm/editor/transform_drivers.h"

namespace rhythm::runtime {
struct NodeOutput;
}
namespace rhythm::studio {
// UI-thread author intent and cached source descriptions. Live values are read
// only while the panel is open; committing remains the application's job.
class TransformAutomation final {
   public:
    OutputEdit Draw(const editor::Snapshot& snapshot, graph::NodeId selected,
                    std::span<const runtime::NodeOutput> outputs, bool current, bool gesture_active,
                    const std::map<std::string, std::string>& text);

   private:
    std::string document_id_{};
    std::uint64_t revision_ = 0;
    graph::NodeId selected_ = 0;
    std::vector<editor::TransformDriver> drivers_{};
    std::map<std::string, double> desired_{};
    std::string error_{};
};
}  // namespace rhythm::studio
