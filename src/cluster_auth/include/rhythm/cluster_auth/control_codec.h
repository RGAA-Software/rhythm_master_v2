#pragma once

#include <array>
#include <span>
#include <variant>

#include "rhythm/cluster_auth/room_authority.h"

namespace rhythm::cluster_auth {
struct JoinRequest {
    Invitation invitation_{};
    std::uint32_t profiles_ = 0;
};
struct ResumeRequest {
    std::uint64_t peer_ = 0;
    std::uint64_t epoch_ = 0;
    security::Token credential_{};
};
struct RefreshRequest {
    std::uint64_t peer_ = 0;
    std::uint64_t epoch_ = 0;
    std::uint64_t sequence_ = 0;
};
struct AdmissionRejected {
    AdmissionError error_ = AdmissionError::kInvalid;
};
using AdmissionMessage =
        std::variant<JoinRequest, SessionGrant, ResumeRequest, RefreshRequest, AdmissionRejected>;
struct AdmissionPacket {
    std::array<std::uint8_t, 100> bytes_{};
    std::size_t size_ = 0;
    std::span<const std::uint8_t> Bytes() const {
        return size_ <= bytes_.size() ? std::span(bytes_).first(size_)
                                      : std::span<const std::uint8_t>{};
    }
};
// Control-stream framing is supplied separately by StreamDecoder. This exact-size
// codec does not authenticate a message; only RoomAuthority plus its TLS binding
// can do that. Payloads contain secrets and must never be written to logs.
std::optional<AdmissionPacket> EncodeAdmission(const AdmissionMessage& message);
std::optional<AdmissionMessage> DecodeAdmission(std::span<const std::uint8_t> bytes);
}  // namespace rhythm::cluster_auth
