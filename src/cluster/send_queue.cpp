#include "rhythm/cluster/send_queue.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace rhythm::cluster {
namespace {
constexpr std::size_t kControlBytes = 64 * 1024;
constexpr std::size_t kAssetBytes = 256 * 1024;
}  // namespace
SendPayload::SendPayload(std::span<const std::uint8_t> bytes) {
    if (bytes.empty() || bytes.size() > 65536) throw std::invalid_argument("cluster.payload_size");
    bytes_ = std::make_shared<const std::vector<std::uint8_t>>(bytes.begin(), bytes.end());
}
std::span<const std::uint8_t> SendPayload::Bytes() const {
    return bytes_ ? std::span<const std::uint8_t>(*bytes_) : std::span<const std::uint8_t>{};
}
SendQueue::SendQueue(std::uint64_t generation) { Reset(generation); }
void SendQueue::Reset(std::uint64_t generation) {
    if (!generation || generation <= generation_ || stats_.in_flight_count_)
        throw std::invalid_argument("cluster.queue_generation");
    Close();
    generation_ = generation;
    next_id_ = 0;
    stats_ = {};
    closed_ = false;
}
QueueResult SendQueue::Enqueue(SendChannel channel, SendPayload payload) {
    if (closed_) return QueueResult::kClosed;
    const auto size = payload.Bytes().size();
    if (!size || (channel != SendChannel::kControl && channel != SendChannel::kRealtime &&
                  channel != SendChannel::kAsset))
        return QueueResult::kInvalid;
    const auto maximum = channel == SendChannel::kControl    ? 4096U
                         : channel == SendChannel::kRealtime ? 1024U
                                                             : 65536U;
    if (size > maximum) return QueueResult::kInvalid;
    if (next_id_ == std::numeric_limits<std::uint64_t>::max()) return QueueResult::kFull;
    const auto& queue = channel == SendChannel::kControl ? control_ : assets_;
    std::size_t count = channel == SendChannel::kRealtime ? 0 : queue.size();
    std::size_t bytes = 0;
    if (channel != SendChannel::kRealtime)
        for (const auto& item : queue) bytes += item.payload_.Bytes().size();
    for (const auto& item : in_flight_) {
        if (item && item->channel_ == channel) {
            ++count;
            bytes += item->payload_.Bytes().size();
        }
    }
    if ((channel == SendChannel::kControl && (count >= 64 || size > kControlBytes - bytes)) ||
        (channel == SendChannel::kAsset && (count >= 8 || size > kAssetBytes - bytes)))
        return QueueResult::kFull;
    PendingSend send{{generation_, next_id_ + 1}, channel, std::move(payload)};
    auto result = QueueResult::kAccepted;
    if (channel == SendChannel::kRealtime) {
        if (realtime_) {
            stats_.pending_bytes_ -= realtime_->payload_.Bytes().size();
            ++stats_.replaced_realtime_;
            result = QueueResult::kReplaced;
        }
        realtime_ = std::move(send);
    } else {
        (channel == SendChannel::kControl ? control_ : assets_).push_back(std::move(send));
    }
    ++next_id_;
    stats_.pending_bytes_ += size;
    stats_.high_water_bytes_ =
            std::max(stats_.high_water_bytes_, stats_.pending_bytes_ + stats_.in_flight_bytes_);
    return result;
}
std::optional<PendingSend> SendQueue::Acquire() {
    if (closed_) return std::nullopt;
    const auto slot = std::find_if(in_flight_.begin(), in_flight_.end(),
                                   [](const auto& value) { return !value.has_value(); });
    if (slot == in_flight_.end()) return std::nullopt;
    const auto realtime_busy = std::any_of(
            in_flight_.begin(), in_flight_.end(),
            [](const auto& value) { return value && value->channel_ == SendChannel::kRealtime; });
    if (!control_.empty()) {
        *slot = std::move(control_.front());
        control_.pop_front();
    } else if (realtime_ && !realtime_busy) {
        *slot = std::move(realtime_);
        realtime_.reset();
    } else if (!assets_.empty()) {
        *slot = std::move(assets_.front());
        assets_.pop_front();
    } else {
        return std::nullopt;
    }
    const auto size = slot->value().payload_.Bytes().size();
    stats_.pending_bytes_ -= size;
    stats_.in_flight_bytes_ += size;
    ++stats_.in_flight_count_;
    return *slot;
}
bool SendQueue::Complete(SendTicket ticket) {
    if (ticket.generation_ != generation_ || !ticket.id_) return false;
    for (auto& value : in_flight_) {
        if (value && value->ticket_ == ticket) {
            stats_.in_flight_bytes_ -= value->payload_.Bytes().size();
            --stats_.in_flight_count_;
            value.reset();
            return true;
        }
    }
    return false;
}
void SendQueue::Close() {
    closed_ = true;
    control_.clear();
    assets_.clear();
    realtime_.reset();
    stats_.pending_bytes_ = 0;
}
SendQueueStats SendQueue::Stats() const { return stats_; }
}  // namespace rhythm::cluster
