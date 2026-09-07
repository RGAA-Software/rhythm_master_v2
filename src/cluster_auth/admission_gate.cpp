#include "rhythm/cluster_auth/admission_gate.h"

#include <stdexcept>

namespace rhythm::cluster_auth {
namespace {
constexpr std::int64_t kMaximumTime = std::int64_t{1} << 52;
}
AdmissionGate::AdmissionGate(std::uint64_t connection, std::int64_t connected_at_us)
    : connection_(connection), last_now_us_(connected_at_us) {
    if (!connection || connected_at_us < 0 || connected_at_us > kMaximumTime - 5'000'000)
        throw std::invalid_argument("room.connection");
    deadline_us_ = connected_at_us + 5'000'000;
}
bool AdmissionGate::Tick(std::int64_t now_us) {
    if (state_ == AdmissionState::kClosed) return false;
    if (now_us < last_now_us_ || now_us > kMaximumTime ||
        (state_ == AdmissionState::kAwaiting && now_us >= deadline_us_) ||
        (state_ == AdmissionState::kAdmitted && now_us >= lease_expires_us_)) {
        state_ = AdmissionState::kClosed;
        return false;
    }
    last_now_us_ = now_us;
    return true;
}
AdmissionDecision AdmissionGate::Reject(AdmissionError error) {
    state_ = AdmissionState::kClosed;
    return {AdmissionRejected{error}, true};
}
AdmissionDecision AdmissionGate::Handle(RoomAuthority& authority, const AdmissionMessage& message,
                                        std::int64_t now_us) {
    if (!Tick(now_us)) return {std::nullopt, true};
    Admission result;
    if (state_ == AdmissionState::kAwaiting) {
        if (std::holds_alternative<JoinRequest>(message)) {
            const auto& join = std::get<JoinRequest>(message);
            result = authority.Join(join.invitation_, connection_, join.profiles_, now_us);
        } else if (std::holds_alternative<ResumeRequest>(message)) {
            const auto& resume = std::get<ResumeRequest>(message);
            if (resume.epoch_ != authority.Epoch()) return Reject(AdmissionError::kInvalid);
            result = authority.Resume(resume.peer_, resume.credential_, connection_, now_us);
        } else
            return Reject(AdmissionError::kInvalid);
    } else {
        if (!std::holds_alternative<RefreshRequest>(message))
            return Reject(AdmissionError::kInvalid);
        const auto& refresh = std::get<RefreshRequest>(message);
        if (refresh.peer_ != peer_ ||
            !authority.AuthorizeControl(peer_, connection_, refresh.epoch_, refresh.sequence_,
                                        now_us))
            return Reject(AdmissionError::kInvalid);
        result = authority.Refresh(peer_, connection_, now_us);
        if (result.error_ == AdmissionError::kRateLimited)
            return {AdmissionRejected{result.error_}, false};
    }
    if (!result.grant_) return Reject(result.error_);
    peer_ = result.grant_->peer_;
    lease_expires_us_ = result.grant_->expires_at_us_;
    state_ = AdmissionState::kAdmitted;
    return {*result.grant_, false};
}
void AdmissionGate::TransportClosed(RoomAuthority& authority) {
    if (peer_) authority.Disconnect(connection_);
    state_ = AdmissionState::kClosed;
}
}  // namespace rhythm::cluster_auth
