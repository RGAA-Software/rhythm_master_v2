#include "rhythm/cluster/clock_exchange.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace rhythm::cluster {
namespace {
constexpr std::int64_t kMaximumTimestamp = std::int64_t{1} << 52;
constexpr std::int64_t kTimeoutUs = 1000000;
bool ValidTime(std::int64_t time) { return time >= 0 && time <= kMaximumTimestamp; }
}  // namespace
ClockExchange::ClockExchange(std::uint64_t epoch) { Reset(epoch); }
void ClockExchange::Reset(std::uint64_t epoch) {
    if (!epoch) throw std::invalid_argument("cluster.epoch");
    if (epoch != epoch_) {
        sequence_ = 0;
        last_local_us_.reset();
    }
    epoch_ = epoch;
    pending_ = {};
}
bool ClockExchange::Advance(std::int64_t local_now_us) {
    if (!ValidTime(local_now_us) || (last_local_us_ && local_now_us < *last_local_us_))
        return false;
    last_local_us_ = local_now_us;
    for (auto& request : pending_)
        if (request && local_now_us - request->local_send_us_ > kTimeoutUs) request.reset();
    return true;
}
std::optional<ClockRequest> ClockExchange::Begin(std::int64_t local_now_us) {
    if (!Advance(local_now_us) || sequence_ == std::numeric_limits<std::uint64_t>::max())
        return std::nullopt;
    const auto slot = std::find_if(pending_.begin(), pending_.end(),
                                   [](const auto& request) { return !request.has_value(); });
    if (slot == pending_.end()) return std::nullopt;
    *slot = ClockRequest{epoch_, ++sequence_, local_now_us};
    return *slot;
}
std::optional<ClockProbe> ClockExchange::Complete(const ClockReply& reply,
                                                  std::int64_t local_now_us) {
    if (!Advance(local_now_us) || reply.epoch_ != epoch_ || !reply.sequence_ ||
        !ValidTime(reply.local_send_us_) || !ValidTime(reply.host_receive_us_) ||
        !ValidTime(reply.host_send_us_) || reply.host_send_us_ < reply.host_receive_us_ ||
        local_now_us < reply.local_send_us_ ||
        reply.host_send_us_ - reply.host_receive_us_ > local_now_us - reply.local_send_us_)
        return std::nullopt;
    for (auto& request : pending_) {
        if (request && request->sequence_ == reply.sequence_ &&
            request->local_send_us_ == reply.local_send_us_) {
            const ClockProbe result{epoch_,
                                    reply.sequence_,
                                    request->local_send_us_,
                                    reply.host_receive_us_,
                                    reply.host_send_us_,
                                    local_now_us};
            request.reset();
            return result;
        }
    }
    return std::nullopt;
}
}  // namespace rhythm::cluster
