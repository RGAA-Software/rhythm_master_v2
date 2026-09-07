#include <iostream>
#include <stdexcept>
#include <vector>

#include "rhythm/cluster_auth/control_codec.h"

namespace {
void Check(bool value) {
    if (!value) throw std::runtime_error("admission.codec_contract");
}
}  // namespace
int main() {
    using namespace rhythm::cluster_auth;
    try {
        constexpr std::array<std::uint8_t, 32> kGolden{'R', 'M', 'A', 'D', 1, 4, 0, 32, 0, 0, 0,
                                                       0,   0,   0,   0,   1, 0, 0, 0,  0, 0, 0,
                                                       0,   7,   0,   0,   0, 0, 0, 0,  0, 9};
        const auto encoded = EncodeAdmission(RefreshRequest{1, 7, 9}).value();
        Check(std::equal(encoded.Bytes().begin(), encoded.Bytes().end(), kGolden.begin(),
                         kGolden.end()));
        RoomAuthority room(7, 3);
        const auto invitation = room.IssueInvitation(0, 60'000'000, 10).value();
        const auto grant = room.Join(invitation, 1, 3, 0).grant_.value();
        const std::array<AdmissionMessage, 5> messages{
                JoinRequest{invitation, 3}, grant, ResumeRequest{grant.peer_, 7, grant.credential_},
                RefreshRequest{grant.peer_, 7, 10}, AdmissionRejected{AdmissionError::kLocked}};
        for (const auto& message : messages) {
            const auto packet = EncodeAdmission(message).value();
            const auto decoded = DecodeAdmission(packet.Bytes()).value();
            Check(decoded.index() == message.index());
            const auto rebuilt = EncodeAdmission(decoded).value();
            Check(packet.size_ == rebuilt.size_ && packet.bytes_ == rebuilt.bytes_);
            for (std::size_t size = 0; size < packet.size_; ++size)
                Check(!DecodeAdmission(packet.Bytes().first(size)));
            std::vector<std::uint8_t> extra(packet.Bytes().begin(), packet.Bytes().end());
            extra.push_back(0);
            Check(!DecodeAdmission(extra));
            auto damaged = packet;
            damaged.bytes_[4] = 2;
            Check(!DecodeAdmission(damaged.Bytes()));
            damaged = packet;
            damaged.bytes_[5] = 0;
            Check(!DecodeAdmission(damaged.Bytes()));
            damaged = packet;
            damaged.bytes_[7] ^= 1;
            Check(!DecodeAdmission(damaged.Bytes()));
            std::uint32_t random = 0x14890;
            for (int iteration = 0; iteration < 2000; ++iteration) {
                damaged = packet;
                random = random * 1664525 + 1013904223;
                const auto index = random % packet.size_;
                random = random * 1664525 + 1013904223;
                damaged.bytes_[index] = static_cast<std::uint8_t>(random >> 24);
                if (const auto parsed = DecodeAdmission(damaged.Bytes())) {
                    const auto canonical = EncodeAdmission(*parsed).value();
                    Check(canonical.size_ == damaged.size_ && canonical.bytes_ == damaged.bytes_);
                }
            }
        }
        Check(!EncodeAdmission(JoinRequest{}));
        Check(!EncodeAdmission(SessionGrant{}));
        Check(!EncodeAdmission(RefreshRequest{1, 7, 0}));
        Check(!EncodeAdmission(AdmissionRejected{AdmissionError::kNone}));
        std::cout << "admission codec passed: exact sizes, fixed wire sample, all truncations, "
                     "versions and 10000 bounded mutations\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
