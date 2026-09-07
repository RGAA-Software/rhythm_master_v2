#include "rhythm/player/prepared_package.h"

#include <picosha2.h>

#include <stdexcept>
#include <utility>

#include "rhythm/prepared_assets/prepare.h"

namespace rhythm::player {
PreparedPackage::PreparedPackage(std::string_view bytes, std::stop_token stop)
    : package_(project::DecodePackage(bytes)),
      resources_(prepared_assets::Prepare(package_->program_, package_->assets_, stop)),
      soundtrack_(
              prepared_assets::PrepareSoundtrack(package_->soundtrack_, package_->assets_, stop)) {
    picosha2::hash256(bytes.begin(), bytes.end(), digest_);
}
PreparedPackage::PreparedPackage(PreparedPackage&& other) noexcept
    : package_(std::exchange(other.package_, std::nullopt)),
      resources_(std::move(other.resources_)),
      soundtrack_(std::move(other.soundtrack_)),
      digest_(other.digest_) {}
PreparedPackage& PreparedPackage::operator=(PreparedPackage&& other) noexcept {
    if (this != &other) {
        package_ = std::exchange(other.package_, std::nullopt);
        resources_ = std::move(other.resources_);
        soundtrack_ = std::move(other.soundtrack_);
        digest_ = other.digest_;
    }
    return *this;
}
std::optional<project::PackageProfile> PreparedPackage::Profile() const {
    if (!package_) return std::nullopt;
    return package_->profile_;
}
bool PreparedPackage::SupportsAnalyticSeek() const {
    if (!package_) return false;
    for (const auto& instruction : package_->program_.instructions_) {
        using graph::Operation;
        switch (instruction.operation_) {
            case Operation::kTime:
            case Operation::kOscillator:
            case Operation::kSample:
            case Operation::kGradient:
            case Operation::kTransform:
            case Operation::kBlend:
            case Operation::kOutput:
            case Operation::kConstant:
            case Operation::kMath:
            case Operation::kLocalTime:
            case Operation::kCurve:
            case Operation::kMap:
            case Operation::kCompare:
            case Operation::kSelect:
            case Operation::kNoise:
            case Operation::kSessionTime:
            case Operation::kParticipantRole:
            case Operation::kSharedControl:
                break;
            default:
                return false;
        }
    }
    return true;
}
project::RuntimePackage PreparedPackage::Take() {
    if (!package_) throw std::invalid_argument("player.empty_prepared_package");
    auto result = std::move(*package_);
    package_.reset();
    return result;
}
}  // namespace rhythm::player
