#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <variant>

#include "rhythm/cluster/input_buffer.h"

namespace rhythm::cluster {
// Clock responses echo the request identity and send time. The receiving host
// stamps local_receive_us itself and matches its outstanding request before
// constructing a ClockProbe; the peer cannot supply that fourth timestamp.
struct ClockRequest {
    std::uint64_t epoch_ = 0;
    std::uint64_t sequence_ = 0;
    std::int64_t local_send_us_ = 0;
};
struct ClockReply {
    std::uint64_t epoch_ = 0;
    std::uint64_t sequence_ = 0;
    std::int64_t local_send_us_ = 0;
    std::int64_t host_receive_us_ = 0;
    std::int64_t host_send_us_ = 0;
};
using RealtimeMessage = std::variant<InputFrame, ClockRequest, ClockReply>;
struct RealtimeDatagram {
    std::array<std::uint8_t, 100> bytes_{};
    std::uint16_t size_ = 0;
    std::span<const std::uint8_t> Bytes() const;
};
// Allocation-free wire v1, network byte order, exact lengths. Normalized
// controls use nearest unsigned 16-bit quantization (error <= 0.5/65535).
// This codec is not authentication, admission control or replay protection.
// Decode only after the transport has authenticated the room/connection;
// callers still enforce epoch, sequence, request matching and time windows.
std::optional<RealtimeDatagram> EncodeRealtime(const RealtimeMessage& message);
std::optional<RealtimeMessage> DecodeRealtime(std::span<const std::uint8_t> bytes);
}  // namespace rhythm::cluster
