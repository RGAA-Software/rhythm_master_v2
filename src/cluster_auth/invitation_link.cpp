#include "rhythm/cluster_auth/invitation_link.h"

#include <algorithm>
#include <span>

namespace rhythm::cluster_auth {
namespace {
constexpr std::string_view kPrefix = "rhythmmaster://join/1/";
constexpr std::string_view kAlphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
constexpr std::size_t kPayloadSize = 140;
constexpr std::size_t kTextSize = (kPayloadSize * 8 + 5) / 6;
bool Any(std::span<const std::uint8_t> bytes) {
    return std::any_of(bytes.begin(), bytes.end(), [](auto byte) { return byte != 0; });
}
bool Valid(const InvitationLink& link) {
    const auto& invitation = link.invitation_;
    if (!invitation.epoch_ || !invitation.generation_ || invitation.expires_at_us_ <= 0 ||
        invitation.expires_at_us_ > (std::int64_t{1} << 52) || !Any(invitation.room_) ||
        !Any(invitation.secret_) || !Any(link.pin_) || !link.endpoint_.port_)
        return false;
    const auto& address = link.endpoint_.address_;
    if (link.endpoint_.family_ == AddressFamily::kIpv4)
        return address[0] != 0 && address[0] < 224 && !Any(std::span(address).subspan(4));
    if (link.endpoint_.family_ != AddressFamily::kIpv6 || !Any(address) || address[0] == 0xff ||
        (address[0] == 0xfe && (address[1] & 0xc0) == 0x80))
        return false;
    // Reject IPv4-mapped literals: clients use the explicit IPv4 form instead.
    return Any(std::span(address).first(10)) || address[10] != 0xff || address[11] != 0xff;
}
void Put64(std::span<std::uint8_t> destination, std::uint64_t value) {
    for (std::size_t index = 0; index < 8; ++index)
        destination[index] = static_cast<std::uint8_t>(value >> (56 - index * 8));
}
std::uint64_t Get64(std::span<const std::uint8_t> source) {
    std::uint64_t value = 0;
    for (std::size_t index = 0; index < 8; ++index) value = (value << 8) | source[index];
    return value;
}
}  // namespace
std::optional<std::string> EncodeInvitation(const InvitationLink& link) {
    if (!Valid(link)) return std::nullopt;
    std::array<std::uint8_t, kPayloadSize> payload{};
    payload[0] = 1;
    payload[1] = static_cast<std::uint8_t>(link.endpoint_.family_);
    payload[2] = static_cast<std::uint8_t>(link.endpoint_.port_ >> 8);
    payload[3] = static_cast<std::uint8_t>(link.endpoint_.port_);
    Put64(std::span(payload).subspan(4), link.invitation_.epoch_);
    Put64(std::span(payload).subspan(12), link.invitation_.generation_);
    Put64(std::span(payload).subspan(20),
          static_cast<std::uint64_t>(link.invitation_.expires_at_us_));
    std::copy(link.endpoint_.address_.begin(), link.endpoint_.address_.end(), payload.begin() + 28);
    std::copy(link.invitation_.room_.begin(), link.invitation_.room_.end(), payload.begin() + 44);
    std::copy(link.invitation_.secret_.begin(), link.invitation_.secret_.end(),
              payload.begin() + 76);
    std::copy(link.pin_.begin(), link.pin_.end(), payload.begin() + 108);
    std::string text(kPrefix);
    text.reserve(kPrefix.size() + kTextSize);
    std::uint32_t buffer = 0;
    int bits = 0;
    for (auto byte : payload) {
        buffer = (buffer << 8) | byte;
        bits += 8;
        while (bits >= 6) {
            bits -= 6;
            text += kAlphabet[(buffer >> bits) & 63];
        }
    }
    if (bits) text += kAlphabet[(buffer << (6 - bits)) & 63];
    return text;
}
std::optional<InvitationLink> DecodeInvitation(std::string_view text) {
    if (text.size() != kPrefix.size() + kTextSize || !text.starts_with(kPrefix))
        return std::nullopt;
    std::array<std::uint8_t, kPayloadSize> payload{};
    std::uint32_t buffer = 0;
    int bits = 0;
    std::size_t position = 0;
    for (char symbol : text.substr(kPrefix.size())) {
        const auto value = kAlphabet.find(symbol);
        if (value == std::string_view::npos) return std::nullopt;
        buffer = (buffer << 6) | static_cast<std::uint32_t>(value);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if (position >= payload.size()) return std::nullopt;
            payload[position++] = static_cast<std::uint8_t>(buffer >> bits);
        }
    }
    if (position != payload.size() || (buffer & ((std::uint32_t{1} << bits) - 1)) != 0 ||
        payload[0] != 1)
        return std::nullopt;
    const auto expires = Get64(std::span(payload).subspan(20));
    if (expires > (std::uint64_t{1} << 52)) return std::nullopt;
    InvitationLink link;
    link.endpoint_.family_ = static_cast<AddressFamily>(payload[1]);
    link.endpoint_.port_ = static_cast<std::uint16_t>((payload[2] << 8) | payload[3]);
    link.invitation_.epoch_ = Get64(std::span(payload).subspan(4));
    link.invitation_.generation_ = Get64(std::span(payload).subspan(12));
    link.invitation_.expires_at_us_ = static_cast<std::int64_t>(expires);
    std::copy_n(payload.begin() + 28, 16, link.endpoint_.address_.begin());
    std::copy_n(payload.begin() + 44, 32, link.invitation_.room_.begin());
    std::copy_n(payload.begin() + 76, 32, link.invitation_.secret_.begin());
    std::copy_n(payload.begin() + 108, 32, link.pin_.begin());
    return Valid(link) ? std::optional(link) : std::nullopt;
}
}  // namespace rhythm::cluster_auth
