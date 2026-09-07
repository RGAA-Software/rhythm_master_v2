#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

#include "rhythm/cluster/datagram_codec.h"

int main() {
    using namespace rhythm::cluster;
    try {
        const auto check = [](bool value) {
            if (!value) throw std::runtime_error("cluster.datagram_contract");
        };
        InputFrame frame{0x0102030405060708, 1, 1000000};
        frame.inputs_.group_ = 0x1234;
        frame.inputs_.index_ = 65535;
        frame.inputs_.controls_[0] = 1;
        frame.inputs_.controls_[1] = 0.5;
        const auto encoded = EncodeRealtime(frame).value();
        std::array<std::uint8_t, 100> golden{'R', 'M', 'R', 'T', 1,   1,   0,   100, 1,   2,
                                             3,   4,   5,   6,   7,   8,   0,   0,   0,   0,
                                             0,   0,   0,   1,   0,   0,   0,   0,   0,   15,
                                             66,  64,  18,  52,  255, 255, 255, 255, 128, 0};
        check(encoded.bytes_ == golden && encoded.size_ == 100);
        const auto decoded = std::get<InputFrame>(DecodeRealtime(golden).value());
        check(decoded.epoch_ == frame.epoch_ && decoded.sequence_ == 1 &&
              decoded.present_us_ == 1000000 && decoded.inputs_.group_ == 0x1234 &&
              decoded.inputs_.index_ == 65535);
        InputBuffer buffer(frame.epoch_);
        check(buffer.Push(decoded, 1000000) == InputResult::kAccepted);
        check(buffer.Push(decoded, 1000000) == InputResult::kReplay);
        check(buffer.Sample(1000000)->inputs_.controls_[0] == 1);
        for (int index = 0; index <= 65535; ++index) {
            frame.inputs_.controls_[0] = static_cast<double>(index) / 65535;
            const auto output = EncodeRealtime(frame).value();
            const auto input = std::get<InputFrame>(DecodeRealtime(output.Bytes()).value());
            check(std::abs(frame.inputs_.controls_[0] - input.inputs_.controls_[0]) <= 0.5 / 65535);
        }
        for (const double invalid : {-0.1, 1.1, std::numeric_limits<double>::infinity(),
                                     std::numeric_limits<double>::quiet_NaN()}) {
            frame.inputs_.controls_[0] = invalid;
            check(!EncodeRealtime(frame));
        }
        frame.inputs_.controls_[0] = 0;
        frame.inputs_.group_ = 65536;
        check(!EncodeRealtime(frame));
        frame.inputs_.group_ = 0;
        frame.present_us_ = -1;
        check(!EncodeRealtime(frame));
        check(!EncodeRealtime(ClockRequest{0, 1, 0}));
        check(!EncodeRealtime(ClockRequest{1, 0, 0}));
        check(!EncodeRealtime(ClockRequest{1, 1, (std::int64_t{1} << 52) + 1}));
        check(!EncodeRealtime(ClockReply{1, 1, 0, 2, 1}));
        check(!EncodeRealtime(ClockReply{1, 1, 0, -1, 1}));
        for (const RealtimeMessage& message :
             {RealtimeMessage{decoded}, RealtimeMessage{ClockRequest{7, 9, 123}},
              RealtimeMessage{ClockReply{7, 9, 123, 1000, 1010}}}) {
            const auto packet = EncodeRealtime(message).value();
            check(EncodeRealtime(DecodeRealtime(packet.Bytes()).value())->bytes_ == packet.bytes_);
            for (std::size_t size = 0; size < packet.size_; ++size)
                check(!DecodeRealtime(packet.Bytes().first(size)));
            auto extra = std::vector(packet.Bytes().begin(), packet.Bytes().end());
            extra.push_back(0);
            check(!DecodeRealtime(extra));
            for (std::size_t index = 0; index < 8; ++index) {
                auto bad = packet;
                bad.bytes_[index] ^= 0x80;
                check(!DecodeRealtime(bad.Bytes()));
            }
            auto bad = packet;
            bad.bytes_[24] = 0xff;
            check(!DecodeRealtime(bad.Bytes()));
        }
        // Deterministic malformed packet corpus. Valid mutations must canonicalize
        // byte-for-byte, regardless of whether a later session accepts their epoch.
        std::uint32_t random = 17;
        for (int trial = 0; trial < 20000; ++trial) {
            auto packet = encoded;
            random = random * 1664525U + 1013904223U;
            packet.bytes_[random % 100] ^= static_cast<std::uint8_t>(random >> 24);
            if (const auto value = DecodeRealtime(packet.Bytes())) {
                const auto canonical = EncodeRealtime(*value);
                check(canonical && canonical->bytes_ == packet.bytes_);
            }
        }
        std::cout << "datagram contracts passed: golden byte order, quantization, exact bounds, "
                     "malformed corpus\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
