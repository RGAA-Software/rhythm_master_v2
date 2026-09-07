#include "rhythm/cluster/datagram_codec.h"

#include <algorithm>
#include <cmath>
#include <type_traits>

namespace rhythm::cluster {
namespace {
constexpr std::uint64_t kMaximumTimestamp = std::uint64_t{1} << 52;
constexpr std::array<std::uint8_t, 4> kMagic{'R', 'M', 'R', 'T'};
bool ValidTime(std::int64_t value) {
    return value >= 0 && static_cast<std::uint64_t>(value) <= kMaximumTimestamp;
}
void Write(std::span<std::uint8_t> bytes, std::size_t offset, std::size_t count,
           std::uint64_t value) {
    for (std::size_t index = 0; index < count; ++index) {
        bytes[offset + count - index - 1] = static_cast<std::uint8_t>(value & 255);
        value >>= 8;
    }
}
std::uint64_t Read(std::span<const std::uint8_t> bytes, std::size_t offset, std::size_t count) {
    std::uint64_t value = 0;
    for (std::size_t index = 0; index < count; ++index)
        value = (value << 8) | bytes[offset + index];
    return value;
}
}  // namespace
std::span<const std::uint8_t> RealtimeDatagram::Bytes() const {
    return std::span(bytes_).first(std::min<std::size_t>(size_, bytes_.size()));
}
std::optional<RealtimeDatagram> EncodeRealtime(const RealtimeMessage& message) {
    return std::visit(
            [](const auto& value) -> std::optional<RealtimeDatagram> {
                using T = std::decay_t<decltype(value)>;
                if (!value.epoch_ || !value.sequence_) return std::nullopt;
                RealtimeDatagram result;
                std::copy(kMagic.begin(), kMagic.end(), result.bytes_.begin());
                result.bytes_[4] = 1;
                Write(result.bytes_, 8, 8, value.epoch_);
                Write(result.bytes_, 16, 8, value.sequence_);
                if constexpr (std::is_same_v<T, InputFrame>) {
                    if (!ValidTime(value.present_us_) || !runtime::ValidInputs(value.inputs_))
                        return std::nullopt;
                    result.size_ = 100;
                    result.bytes_[5] = 1;
                    Write(result.bytes_, 24, 8, static_cast<std::uint64_t>(value.present_us_));
                    Write(result.bytes_, 32, 2, value.inputs_.group_);
                    Write(result.bytes_, 34, 2, value.inputs_.index_);
                    for (std::size_t index = 0; index < value.inputs_.controls_.size(); ++index)
                        Write(result.bytes_, 36 + index * 2, 2,
                              static_cast<std::uint64_t>(
                                      std::floor(value.inputs_.controls_[index] * 65535 + 0.5)));
                } else {
                    if (!ValidTime(value.local_send_us_)) return std::nullopt;
                    result.size_ = 32;
                    result.bytes_[5] = 2;
                    Write(result.bytes_, 24, 8, static_cast<std::uint64_t>(value.local_send_us_));
                    if constexpr (std::is_same_v<T, ClockReply>) {
                        if (!ValidTime(value.host_receive_us_) || !ValidTime(value.host_send_us_) ||
                            value.host_send_us_ < value.host_receive_us_)
                            return std::nullopt;
                        result.size_ = 48;
                        result.bytes_[5] = 3;
                        Write(result.bytes_, 32, 8,
                              static_cast<std::uint64_t>(value.host_receive_us_));
                        Write(result.bytes_, 40, 8,
                              static_cast<std::uint64_t>(value.host_send_us_));
                    }
                }
                Write(result.bytes_, 6, 2, result.size_);
                return result;
            },
            message);
}
std::optional<RealtimeMessage> DecodeRealtime(std::span<const std::uint8_t> bytes) {
    if (bytes.size() < 8 || bytes.size() > 100 ||
        !std::equal(kMagic.begin(), kMagic.end(), bytes.begin()) || bytes[4] != 1)
        return std::nullopt;
    const auto kind = bytes[5];
    const std::size_t expected = kind == 1 ? 100 : kind == 2 ? 32 : kind == 3 ? 48 : 0;
    if (!expected || bytes.size() != expected || Read(bytes, 6, 2) != expected) return std::nullopt;
    const auto epoch = Read(bytes, 8, 8);
    const auto sequence = Read(bytes, 16, 8);
    const auto time = Read(bytes, 24, 8);
    if (!epoch || !sequence || time > kMaximumTimestamp) return std::nullopt;
    if (kind == 1) {
        InputFrame frame{epoch, sequence, static_cast<std::int64_t>(time)};
        frame.inputs_.group_ = static_cast<std::uint32_t>(Read(bytes, 32, 2));
        frame.inputs_.index_ = static_cast<std::uint32_t>(Read(bytes, 34, 2));
        for (std::size_t index = 0; index < frame.inputs_.controls_.size(); ++index)
            frame.inputs_.controls_[index] =
                    static_cast<double>(Read(bytes, 36 + index * 2, 2)) / 65535;
        return frame;
    }
    if (kind == 2) return ClockRequest{epoch, sequence, static_cast<std::int64_t>(time)};
    const auto received = Read(bytes, 32, 8);
    const auto sent = Read(bytes, 40, 8);
    if (received > kMaximumTimestamp || sent > kMaximumTimestamp || sent < received)
        return std::nullopt;
    return ClockReply{epoch, sequence, static_cast<std::int64_t>(time),
                      static_cast<std::int64_t>(received), static_cast<std::int64_t>(sent)};
}
}  // namespace rhythm::cluster
