#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace rhythm::cluster_auth {
struct SessionEnvelope {
    std::uint64_t epoch_ = 0;
    std::uint64_t peer_ = 0;
    std::uint64_t sequence_ = 0;
    std::vector<std::uint8_t> body_{};
};
// Exact v1 reliable application envelope, at most 4096 bytes including its
// 32-byte header. This decoder does not authorize a connection or body semantics.
// Participant sequence is shared with credential refresh and survives resume;
// host sequence is independent and scoped to one authenticated TLS connection.
std::optional<std::vector<std::uint8_t>> EncodeSessionEnvelope(const SessionEnvelope& message);
std::optional<SessionEnvelope> DecodeSessionEnvelope(std::span<const std::uint8_t> bytes);
}  // namespace rhythm::cluster_auth
