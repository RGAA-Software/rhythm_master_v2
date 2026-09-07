#pragma once

#include "rhythm/qr/reader.h"

namespace rhythm::qr::detail {
inline bool ValidLuminance(const LuminanceView& frame) {
    return frame.width_ && frame.height_ && frame.width_ <= 2048 && frame.height_ <= 2048 &&
           std::uint64_t{frame.width_} * frame.height_ <= 1'048'576 &&
           frame.row_stride_ >= frame.width_ && frame.row_stride_ <= 4096 &&
           frame.bytes_.size() <= 4 * 1024 * 1024 &&
           frame.bytes_.size() >= std::uint64_t{frame.row_stride_} * frame.height_;
}
}  // namespace rhythm::qr::detail
