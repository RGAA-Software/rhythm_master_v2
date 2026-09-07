#include "rhythm/cluster_auth/control_codec.h"

#include <algorithm>

namespace rhythm::cluster_auth {
namespace {
constexpr std::int64_t kMaximumTime = std::int64_t{1} << 52;
class Writer final {
   public:
    explicit Writer(std::uint8_t kind, std::size_t size) {
        packet_.size_ = size;
        packet_.bytes_[0] = 'R';
        packet_.bytes_[1] = 'M';
        packet_.bytes_[2] = 'A';
        packet_.bytes_[3] = 'D';
        packet_.bytes_[4] = 1;
        packet_.bytes_[5] = kind;
        packet_.bytes_[6] = 0;
        packet_.bytes_[7] = static_cast<std::uint8_t>(size);
    }
    void Integer(std::uint64_t value, std::size_t count = 8) {
        for (std::size_t index = 0; index < count; ++index)
            packet_.bytes_[position_++] =
                    static_cast<std::uint8_t>(value >> ((count - index - 1) * 8));
    }
    void Token(const security::Token& token) {
        std::copy(token.begin(), token.end(),
                  packet_.bytes_.begin() + static_cast<std::ptrdiff_t>(position_));
        position_ += token.size();
    }
    AdmissionPacket Finish() const { return packet_; }

   private:
    AdmissionPacket packet_{};
    std::size_t position_ = 8;
};
class Reader final {
   public:
    explicit Reader(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}
    std::uint64_t Integer(std::size_t count = 8) {
        std::uint64_t value = 0;
        for (std::size_t index = 0; index < count; ++index)
            value = (value << 8) | bytes_[position_++];
        return value;
    }
    security::Token Token() {
        security::Token token{};
        std::copy_n(bytes_.begin() + static_cast<std::ptrdiff_t>(position_), token.size(),
                    token.begin());
        position_ += token.size();
        return token;
    }

   private:
    std::span<const std::uint8_t> bytes_{};
    std::size_t position_ = 8;
};
bool Time(std::int64_t value) { return value > 0 && value <= kMaximumTime; }
}  // namespace
std::optional<AdmissionPacket> EncodeAdmission(const AdmissionMessage& message) {
    if (std::holds_alternative<JoinRequest>(message)) {
        const auto& join = std::get<JoinRequest>(message);
        const auto& invitation = join.invitation_;
        if (!invitation.epoch_ || !invitation.generation_ || !Time(invitation.expires_at_us_) ||
            !join.profiles_)
            return std::nullopt;
        Writer writer(1, 100);
        writer.Integer(invitation.epoch_);
        writer.Integer(invitation.generation_);
        writer.Integer(static_cast<std::uint64_t>(invitation.expires_at_us_));
        writer.Token(invitation.room_);
        writer.Token(invitation.secret_);
        writer.Integer(join.profiles_, 4);
        return writer.Finish();
    }
    if (std::holds_alternative<SessionGrant>(message)) {
        const auto& grant = std::get<SessionGrant>(message);
        if (!grant.peer_ || !grant.epoch_ || !grant.profiles_ || !Time(grant.expires_at_us_))
            return std::nullopt;
        Writer writer(2, 76);
        writer.Integer(grant.peer_);
        writer.Integer(grant.epoch_);
        writer.Token(grant.credential_);
        writer.Integer(grant.profiles_, 4);
        writer.Integer(static_cast<std::uint64_t>(grant.expires_at_us_));
        writer.Integer(grant.last_control_sequence_);
        return writer.Finish();
    }
    if (std::holds_alternative<ResumeRequest>(message)) {
        const auto& resume = std::get<ResumeRequest>(message);
        if (!resume.peer_ || !resume.epoch_) return std::nullopt;
        Writer writer(3, 56);
        writer.Integer(resume.peer_);
        writer.Integer(resume.epoch_);
        writer.Token(resume.credential_);
        return writer.Finish();
    }
    if (std::holds_alternative<RefreshRequest>(message)) {
        const auto& refresh = std::get<RefreshRequest>(message);
        if (!refresh.peer_ || !refresh.epoch_ || !refresh.sequence_) return std::nullopt;
        Writer writer(4, 32);
        writer.Integer(refresh.peer_);
        writer.Integer(refresh.epoch_);
        writer.Integer(refresh.sequence_);
        return writer.Finish();
    }
    const auto error = std::get<AdmissionRejected>(message).error_;
    if (error < AdmissionError::kInvalid || error > AdmissionError::kIncompatible)
        return std::nullopt;
    Writer writer(5, 9);
    writer.Integer(static_cast<std::uint8_t>(error), 1);
    return writer.Finish();
}
std::optional<AdmissionMessage> DecodeAdmission(std::span<const std::uint8_t> bytes) {
    constexpr std::array<std::uint8_t, 5> kHeader{'R', 'M', 'A', 'D', 1};
    if (bytes.size() < 8 || bytes.size() > 100 ||
        !std::equal(kHeader.begin(), kHeader.end(), bytes.begin()) || bytes[6] != 0 ||
        bytes[7] != bytes.size())
        return std::nullopt;
    constexpr std::array<std::size_t, 6> kSizes{0, 100, 76, 56, 32, 9};
    if (!bytes[5] || bytes[5] >= kSizes.size() || bytes.size() != kSizes[bytes[5]])
        return std::nullopt;
    Reader reader(bytes);
    AdmissionMessage message;
    switch (bytes[5]) {
        case 1: {
            JoinRequest join;
            join.invitation_.epoch_ = reader.Integer();
            join.invitation_.generation_ = reader.Integer();
            const auto expires = reader.Integer();
            if (expires > static_cast<std::uint64_t>(kMaximumTime)) return std::nullopt;
            join.invitation_.expires_at_us_ = static_cast<std::int64_t>(expires);
            join.invitation_.room_ = reader.Token();
            join.invitation_.secret_ = reader.Token();
            join.profiles_ = static_cast<std::uint32_t>(reader.Integer(4));
            message = join;
            break;
        }
        case 2: {
            SessionGrant grant;
            grant.peer_ = reader.Integer();
            grant.epoch_ = reader.Integer();
            grant.credential_ = reader.Token();
            grant.profiles_ = static_cast<std::uint32_t>(reader.Integer(4));
            const auto expires = reader.Integer();
            if (expires > static_cast<std::uint64_t>(kMaximumTime)) return std::nullopt;
            grant.expires_at_us_ = static_cast<std::int64_t>(expires);
            grant.last_control_sequence_ = reader.Integer();
            message = grant;
            break;
        }
        case 3:
            message = ResumeRequest{reader.Integer(), reader.Integer(), reader.Token()};
            break;
        case 4:
            message = RefreshRequest{reader.Integer(), reader.Integer(), reader.Integer()};
            break;
        case 5:
            message = AdmissionRejected{static_cast<AdmissionError>(reader.Integer(1))};
            break;
        default:
            return std::nullopt;
    }
    if (!EncodeAdmission(message)) return std::nullopt;
    return message;
}
}  // namespace rhythm::cluster_auth
