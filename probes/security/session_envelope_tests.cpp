#include <iostream>
#include <stdexcept>

#include "rhythm/cluster_auth/session_envelope.h"

namespace {
void Require(bool condition) {
    if (!condition) throw std::runtime_error("session.envelope_contract");
}
}  // namespace
int main() {
    using namespace rhythm::cluster_auth;
    try {
        SessionEnvelope message{1, 2, 3, {0xab, 0xcd}};
        const auto encoded = EncodeSessionEnvelope(message);
        Require(encoded.has_value());
        const std::vector<std::uint8_t> golden{'R', 'M', 'R', 'C', 1, 0, 0, 34, 0,    0,   0, 0,
                                               0,   0,   0,   1,   0, 0, 0, 0,  0,    0,   0, 2,
                                               0,   0,   0,   0,   0, 0, 0, 3,  0xab, 0xcd};
        Require(*encoded == golden);
        for (std::size_t length = 0; length < golden.size(); ++length)
            Require(!DecodeSessionEnvelope(std::span(golden).first(length)));
        auto extended = golden;
        extended.push_back(0);
        Require(!DecodeSessionEnvelope(extended));
        for (std::size_t index = 0; index < 8; ++index) {
            auto invalid = golden;
            invalid[index] ^= 1;
            Require(!DecodeSessionEnvelope(invalid));
        }
        message.body_.resize(4064);
        Require(EncodeSessionEnvelope(message)->size() == 4096);
        message.body_.push_back(0);
        Require(!EncodeSessionEnvelope(message));
        message.body_.clear();
        Require(!EncodeSessionEnvelope(message));
        std::uint32_t state = 7;
        for (int repeat = 0; repeat < 10000; ++repeat) {
            auto changed = golden;
            state = state * 1664525U + 1013904223U;
            changed[state % changed.size()] ^= static_cast<std::uint8_t>(state >> 24);
            if (const auto decoded = DecodeSessionEnvelope(changed))
                Require(EncodeSessionEnvelope(*decoded) == changed);
        }
        std::cout << "Session envelope passed: exact golden bytes, bounds, truncation, versions "
                     "and mutations\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
