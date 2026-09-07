#include "rhythm/cluster_auth/session_envelope.h"

#include <algorithm>
#include <array>

namespace rhythm::cluster_auth {
namespace {
constexpr std::array<std::uint8_t, 6> kPrefix{'R', 'M', 'R', 'C', 1, 0};
void Put(std::span<std::uint8_t> bytes, std::uint64_t value) {
    for (auto iterator = bytes.rbegin(); iterator != bytes.rend(); ++iterator) {
        *iterator = static_cast<std::uint8_t>(value);
        value >>= 8;
    }
}
std::uint64_t Get(std::span<const std::uint8_t> bytes) {
    std::uint64_t value = 0;
    for (const auto byte : bytes) value = (value << 8) | byte;
    return value;
}
}  // namespace
std::optional<std::vector<std::uint8_t>> EncodeSessionEnvelope(const SessionEnvelope& message) {
    if (!message.epoch_ || !message.peer_ || !message.sequence_ || message.body_.empty() ||
        message.body_.size() > 4064)
        return {};
    std::vector<std::uint8_t> bytes(32 + message.body_.size());
    std::copy(kPrefix.begin(), kPrefix.end(), bytes.begin());
    auto target = std::span(bytes);
    Put(target.subspan(6, 2), bytes.size());
    Put(target.subspan(8, 8), message.epoch_);
    Put(target.subspan(16, 8), message.peer_);
    Put(target.subspan(24, 8), message.sequence_);
    std::copy(message.body_.begin(), message.body_.end(), bytes.begin() + 32);
    return bytes;
}
std::optional<SessionEnvelope> DecodeSessionEnvelope(std::span<const std::uint8_t> bytes) {
    if (bytes.size() <= 32 || bytes.size() > 4096 ||
        !std::equal(kPrefix.begin(), kPrefix.end(), bytes.begin()) ||
        Get(bytes.subspan(6, 2)) != bytes.size())
        return {};
    SessionEnvelope message;
    message.epoch_ = Get(bytes.subspan(8, 8));
    message.peer_ = Get(bytes.subspan(16, 8));
    message.sequence_ = Get(bytes.subspan(24, 8));
    if (!message.epoch_ || !message.peer_ || !message.sequence_) return {};
    message.body_.assign(bytes.begin() + 32, bytes.end());
    return message;
}
}  // namespace rhythm::cluster_auth
