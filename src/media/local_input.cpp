#include "local_input.h"

#include <cerrno>
#include <limits>
#include <stdexcept>

namespace rhythm::media::detail {
LocalInput::LocalInput(const std::filesystem::path& path, std::stop_token stop) : stop_(stop) {
    if (stop_.stop_requested()) {
        throw std::runtime_error("media operation canceled");
    }
    if (!std::filesystem::is_regular_file(path)) {
        throw std::runtime_error("media source must be a local regular file");
    }
    const auto file_size = std::filesystem::file_size(path);
    if (file_size == 0 ||
        file_size > static_cast<std::uintmax_t>(std::numeric_limits<std::int64_t>::max())) {
        throw std::runtime_error("invalid media file size");
    }
    size_ = static_cast<std::int64_t>(file_size);
    file_.open(path, std::ios::binary);
    if (!file_) {
        throw std::runtime_error("cannot open media file");
    }
    constexpr int kBufferBytes = 32768;
    auto buffer = std::unique_ptr<unsigned char, BufferDelete>(
            static_cast<unsigned char*>(av_malloc(kBufferBytes)));
    if (!buffer) {
        throw std::bad_alloc();
    }
    context_.reset(avio_alloc_context(buffer.get(), kBufferBytes, 0, this, Read, nullptr, Seek));
    if (!context_) {
        throw std::bad_alloc();
    }
    // Ownership of the buffer transfers to the context's RAII deleter.
    buffer.release();
}

int LocalInput::Read(void* opaque, std::uint8_t* buffer, int size) noexcept {
    auto& input = *static_cast<LocalInput*>(opaque);
    if (input.Canceled()) {
        return AVERROR_EXIT;
    }
    if (size <= 0) {
        return AVERROR(EINVAL);
    }
    input.file_.read(reinterpret_cast<char*>(buffer), size);
    const auto count = input.file_.gcount();
    if (input.file_.bad()) {
        return AVERROR(EIO);
    }
    return count > 0 ? static_cast<int>(count) : AVERROR_EOF;
}

std::int64_t LocalInput::Seek(void* opaque, std::int64_t offset, int whence) noexcept {
    auto& input = *static_cast<LocalInput*>(opaque);
    if (input.Canceled()) {
        return AVERROR_EXIT;
    }
    if (whence == AVSEEK_SIZE) {
        return input.size_;
    }
    whence &= ~AVSEEK_FORCE;
    const auto direction = whence == SEEK_SET   ? std::ios::beg
                           : whence == SEEK_CUR ? std::ios::cur
                                                : std::ios::end;
    if (whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END) {
        return AVERROR(EINVAL);
    }
    input.file_.clear();
    input.file_.seekg(offset, direction);
    if (!input.file_) {
        return AVERROR(EIO);
    }
    return static_cast<std::int64_t>(input.file_.tellg());
}
}  // namespace rhythm::media::detail
