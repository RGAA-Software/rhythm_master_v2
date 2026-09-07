#include "rhythm/cluster/stream_decoder.h"

#include <algorithm>
#include <stdexcept>

namespace rhythm::cluster {
StreamDecoder::StreamDecoder(SendChannel channel) {
    if (channel == SendChannel::kControl) {
        maximum_message_ = 4096;
        maximum_bytes_ = 65536;
        maximum_count_ = 64;
    } else if (channel == SendChannel::kAsset) {
        maximum_message_ = 65536;
        maximum_bytes_ = 262144;
        maximum_count_ = 8;
    } else {
        throw std::invalid_argument("cluster.stream_channel");
    }
}
StreamFeedResult StreamDecoder::Feed(std::span<const std::uint8_t> bytes) {
    if (closed_) return {StreamFeedStatus::kClosed, 0};
    std::size_t consumed = 0;
    while (consumed < bytes.size()) {
        if (ready_.size() >= maximum_count_) return {StreamFeedStatus::kBackpressure, consumed};
        while (header_used_ < header_.size() && consumed < bytes.size())
            header_[header_used_++] = bytes[consumed++];
        if (header_used_ < header_.size()) break;
        if (!expected_) {
            std::uint32_t length = 0;
            for (const auto value : header_) length = (length << 8) | value;
            if (!length || length > maximum_message_) {
                Cancel();
                return {StreamFeedStatus::kInvalid, consumed};
            }
            expected_ = length;
        }
        if (expected_ > maximum_bytes_ - queued_bytes_)
            return {StreamFeedStatus::kBackpressure, consumed};
        if (body_.size() != expected_) body_.resize(expected_);
        const auto count = std::min(bytes.size() - consumed, expected_ - body_used_);
        std::copy_n(bytes.begin() + static_cast<std::ptrdiff_t>(consumed), count,
                    body_.begin() + static_cast<std::ptrdiff_t>(body_used_));
        consumed += count;
        body_used_ += count;
        if (body_used_ == expected_) {
            ready_.emplace_back(std::span<const std::uint8_t>(body_));
            queued_bytes_ += expected_;
            body_used_ = 0;
            header_used_ = 0;
            expected_ = 0;
        }
    }
    return {StreamFeedStatus::kConsumed, consumed};
}
std::optional<SendPayload> StreamDecoder::Take() {
    if (ready_.empty()) return std::nullopt;
    auto result = std::move(ready_.front());
    ready_.pop_front();
    queued_bytes_ -= result.Bytes().size();
    return result;
}
bool StreamDecoder::Finish() {
    if (closed_) return false;
    if (header_used_ || body_used_) {
        Cancel();
        return false;
    }
    closed_ = true;
    return true;
}
void StreamDecoder::Cancel() {
    closed_ = true;
    ready_.clear();
    body_.clear();
    header_used_ = 0;
    body_used_ = 0;
    expected_ = 0;
    queued_bytes_ = 0;
}
}  // namespace rhythm::cluster
