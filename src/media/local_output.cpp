#include "local_output.h"

#include <algorithm>
#include <cerrno>
#include <limits>

namespace rhythm::media::detail {
namespace {
constexpr std::int64_t kMaximumBytes = 64LL * 1024 * 1024 * 1024;
}
LocalOutput::LocalOutput(const std::filesystem::path& path, std::stop_token stop) : stop_(stop) {
    if (Canceled()) throw std::runtime_error("media.export_canceled");
    if (std::filesystem::exists(path)) throw std::runtime_error("media.export_exists");
    file_.open(path, std::ios::binary);
    if (!file_) throw std::runtime_error("media.export_open");
    constexpr int kBufferBytes = 32768;
    auto buffer = std::unique_ptr<unsigned char, BufferDelete>(
            static_cast<unsigned char*>(av_malloc(kBufferBytes)));
    if (!buffer) throw std::bad_alloc();
    context_.reset(avio_alloc_context(buffer.get(), kBufferBytes, 1, this, nullptr, Write, Seek));
    if (!context_) throw std::bad_alloc();
    buffer.release();
}
int LocalOutput::Write(void* opaque, std::uint8_t* buffer, int size) noexcept {
    auto& output = *static_cast<LocalOutput*>(opaque);
    if (output.Canceled()) return AVERROR_EXIT;
    if (size <= 0 || size > kMaximumBytes - output.position_) return AVERROR(EINVAL);
    output.file_.write(reinterpret_cast<const char*>(buffer), size);
    if (!output.file_) return AVERROR(EIO);
    output.position_ += size;
    output.size_ = std::max(output.size_, output.position_);
    return size;
}
std::int64_t LocalOutput::Seek(void* opaque, std::int64_t offset, int whence) noexcept {
    auto& output = *static_cast<LocalOutput*>(opaque);
    if (output.Canceled()) return AVERROR_EXIT;
    if (whence == AVSEEK_SIZE) return output.size_;
    whence &= ~AVSEEK_FORCE;
    if (whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END) return AVERROR(EINVAL);
    const auto base = whence == SEEK_SET ? 0 : whence == SEEK_CUR ? output.position_ : output.size_;
    if (offset < -base || offset > kMaximumBytes - base) return AVERROR(EINVAL);
    output.file_.seekp(base + offset, std::ios::beg);
    if (!output.file_) return AVERROR(EIO);
    output.position_ = base + offset;
    return output.position_;
}
void LocalOutput::Flush() {
    if (Canceled()) throw std::runtime_error("media.export_canceled");
    avio_flush(context_.get());
    Check(context_->error, "flush encoded media");
    file_.flush();
    if (!file_) throw std::runtime_error("media.export_write");
}
}  // namespace rhythm::media::detail
