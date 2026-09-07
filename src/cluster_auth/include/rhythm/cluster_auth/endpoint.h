#pragma once

#include <array>
#include <cstdint>

namespace rhythm::cluster_auth {
enum class AddressFamily : std::uint8_t { kIpv4 = 4, kIpv6 = 6 };
struct Endpoint {
    AddressFamily family_ = AddressFamily::kIpv4;
    // IPv4 occupies the first four bytes; the remaining twelve must be zero.
    std::array<std::uint8_t, 16> address_{};
    std::uint16_t port_ = 0;
    bool operator==(const Endpoint&) const = default;
};
}  // namespace rhythm::cluster_auth
