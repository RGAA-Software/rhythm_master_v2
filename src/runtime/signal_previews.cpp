#include "rhythm/runtime/signal_previews.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace rhythm::runtime {
void SignalPreviews::Clear() {
    traces_.clear();
    last_seconds_.reset();
}
void SignalPreviews::Capture(const FrameResult& frame, std::span<const graph::NodeId> nodes,
                             double seconds, std::uint64_t reset_generation) {
    if (!std::isfinite(seconds) || seconds < 0) throw std::invalid_argument("viewer.time");
    if (nodes.size() > kMaxPreviews) throw std::length_error("viewer.limit");
    if (reset_generation_ != reset_generation || (last_seconds_ && seconds < *last_seconds_))
        Clear();
    reset_generation_ = reset_generation;
    last_seconds_ = seconds;
    std::erase_if(traces_, [&](const auto& entry) {
        return std::find(nodes.begin(), nodes.end(), entry.first) == nodes.end();
    });
    for (const auto node : nodes) {
        const auto source = std::find_if(frame.outputs_.begin(), frame.outputs_.end(),
                                         [&](const auto& value) { return value.node_ == node; });
        if (frame.budget_ || source == frame.outputs_.end() || !std::isfinite(source->scalar_)) {
            traces_.erase(node);
            continue;
        }
        auto& trace = traces_[node];
        trace.value_ = source->scalar_;
        // Keep plotting arithmetic finite even for valid extreme expression results.
        const auto value = static_cast<float>(std::clamp(trace.value_, -1e30, 1e30));
        if (trace.count_ && seconds - trace.sampled_seconds_ + 1e-9 < 1.0 / 15.0) {
            trace.samples_[(trace.offset_ + trace.count_ - 1) % SignalTrace::kCapacity] = value;
            continue;
        }
        if (trace.count_ < SignalTrace::kCapacity)
            trace.samples_[trace.count_++] = value;
        else {
            trace.samples_[trace.offset_] = value;
            trace.offset_ = (trace.offset_ + 1) % SignalTrace::kCapacity;
        }
        trace.sampled_seconds_ = seconds;
    }
}
}  // namespace rhythm::runtime
