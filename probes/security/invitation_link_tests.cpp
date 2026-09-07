#include <iostream>
#include <stdexcept>

#include "rhythm/cluster_auth/invitation_link.h"

namespace {
void Check(bool value) {
    if (!value) throw std::runtime_error("invitation.codec_contract");
}
}  // namespace
int main() {
    using namespace rhythm::cluster_auth;
    try {
        InvitationLink golden;
        golden.invitation_.epoch_ = 9;
        golden.invitation_.generation_ = 1;
        golden.invitation_.expires_at_us_ = 60'000'000;
        golden.invitation_.room_.fill(1);
        golden.invitation_.secret_.fill(2);
        golden.pin_.fill(3);
        golden.endpoint_ = {AddressFamily::kIpv4, {192, 168, 31, 6}, 4443};
        // Independent Python struct.pack('>BBHQQQ') / urlsafe_b64encode fixture.
        constexpr std::string_view kGolden =
                "rhythmmaster://join/1/AQQRWwAAAAAAAAAJAAAAAAAAAAEAAAAAA5OHAMCoHwYAAAAAAAAAAA"
                "AAAAABAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQICAgICAgICAgICAgICAgICAgICAg"
                "ICAgICAgICAgICAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwM";
        Check(EncodeInvitation(golden).value() == kGolden);
        Check(DecodeInvitation(kGolden)->endpoint_ == golden.endpoint_);
        RoomAuthority room(9, 1);
        InvitationLink link{room.IssueInvitation(0, 60'000'000, 10).value(),
                            rhythm::security::HostIdentity::Create().Fingerprint(),
                            {AddressFamily::kIpv4, {192, 168, 31, 6}, 4443}};
        const auto text = EncodeInvitation(link).value();
        Check(text.size() == 209);
        const auto decoded = DecodeInvitation(text).value();
        Check(decoded.endpoint_ == link.endpoint_ && decoded.pin_ == link.pin_);
        Check(decoded.invitation_.secret_ == link.invitation_.secret_);
        Check(room.Join(decoded.invitation_, 1, 1, 0).grant_.has_value());
        Check(EncodeInvitation(decoded).value() == text);
        Check(!DecodeInvitation(text + "=") && !DecodeInvitation(text + "?x=1"));
        for (std::size_t length = 0; length < text.size(); ++length)
            Check(!DecodeInvitation(std::string_view(text).substr(0, length)));
        auto invalid = text;
        invalid.back() = '!';
        Check(!DecodeInvitation(invalid));
        invalid = text;
        constexpr std::string_view kAlphabet =
                "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
        invalid.back() = kAlphabet[kAlphabet.find(text.back()) | 1];
        Check(!DecodeInvitation(invalid));  // Nonzero unused bits are not another spelling.
        link.endpoint_.address_[4] = 1;
        Check(!EncodeInvitation(link));
        link.endpoint_ = {AddressFamily::kIpv4, {224, 0, 0, 1}, 4443};
        Check(!EncodeInvitation(link));
        link.endpoint_ = {AddressFamily::kIpv6, {0xfd, 1}, 4443};
        Check(DecodeInvitation(EncodeInvitation(link).value())->endpoint_ == link.endpoint_);
        link.endpoint_.address_ = {0xfe, 0x80};
        Check(!EncodeInvitation(link));
        link.endpoint_.address_ = {0xff, 2};
        Check(!EncodeInvitation(link));
        std::uint32_t random = 0x739abc;
        for (int iteration = 0; iteration < 10000; ++iteration) {
            auto mutated = text;
            random = random * 1664525 + 1013904223;
            const auto index = random % mutated.size();
            random = random * 1664525 + 1013904223;
            mutated[index] = static_cast<char>(random >> 24);
            if (auto value = DecodeInvitation(mutated))
                Check(EncodeInvitation(*value).value() == mutated);
        }
        std::cout << "invitation codec passed: bounded canonical links, IPv4/IPv6, strict tails "
                     "and mutation round trips\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
