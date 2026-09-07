#pragma once

#include <array>
#include <optional>
#include <stop_token>

#include "rhythm/prepared_assets/prepare.h"
#include "rhythm/project/package.h"

namespace rhythm::player {
// Validated, move-only package ownership. Construction performs bounded package
// validation and SHA-256 on a worker; it never touches a renderer or UI state.
// Only Session consumes the result, at a host-thread frame boundary. A moved-
// from value is explicitly empty and cannot bypass validation on a second load.
class PreparedPackage final {
   public:
    explicit PreparedPackage(std::string_view bytes, std::stop_token stop = {});
    explicit PreparedPackage(storage::FileBytes bytes, std::stop_token stop = {});
    PreparedPackage(PreparedPackage&& other) noexcept;
    PreparedPackage& operator=(PreparedPackage&& other) noexcept;
    PreparedPackage(const PreparedPackage&) = delete;
    PreparedPackage& operator=(const PreparedPackage&) = delete;
    bool Ready() const { return package_.has_value(); }
    const std::array<std::uint8_t, 32>& Digest() const { return digest_; }
    std::optional<project::PackageProfile> Profile() const;
    // Conservative for known stateless operators. Feedback and future
    // unclassified operations require scene-boundary/snapshot recovery.
    bool SupportsAnalyticSeek() const;

   private:
    friend class Session;
    project::RuntimePackage Take();
    std::optional<project::RuntimePackage> package_{};
    std::shared_ptr<const prepared_assets::Resources> resources_{};
    std::optional<media::SoundtrackSource> soundtrack_{};
    std::array<std::uint8_t, 32> digest_{};
};
}  // namespace rhythm::player
