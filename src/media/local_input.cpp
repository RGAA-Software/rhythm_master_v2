#include "local_input.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
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
    InitializeContext();
}
LocalInput::LocalInput(std::shared_ptr<const std::vector<std::uint8_t>> bytes, std::stop_token stop)
    : bytes_(std::move(bytes)), stop_(stop) {
    if (stop.stop_requested()) throw std::runtime_error("media operation canceled");
    if (!bytes_ || bytes_->empty() || bytes_->size() > 16 * 1024 * 1024)
        throw std::invalid_argument("embedded media byte budget");
    size_ = static_cast<std::int64_t>(bytes_->size());
    InitializeContext();
}
void LocalInput::InitializeContext() {
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

LocalInput::LocalInput(storage::FileBytes bytes, std::stop_token stop)
    : file_bytes_(std::move(bytes)), stop_(stop) {
    if (stop.stop_requested()) throw std::runtime_error("media operation canceled");
    if (!file_bytes_.Valid() || !file_bytes_.Size() ||
        file_bytes_.Size() > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
        throw std::invalid_argument("media file range");
    size_ = static_cast<std::int64_t>(file_bytes_.Size());
    InitializeContext();
}

int LocalInput::Read(void* opaque, std::uint8_t* buffer, int size) noexcept {
    auto& input = *static_cast<LocalInput*>(opaque);
    if (input.Canceled()) {
        return AVERROR_EXIT;
    }
    if (size <= 0) {
        return AVERROR(EINVAL);
    }
    if (input.file_bytes_.Valid()) {
        try {
            const auto count = input.file_bytes_.Read(static_cast<std::uint64_t>(input.position_),
                                                      {buffer, static_cast<std::size_t>(size)});
            input.position_ += static_cast<std::int64_t>(count);
            return count ? static_cast<int>(count) : AVERROR_EOF;
        } catch (const std::exception&) {
            return AVERROR(EIO);
        }
    }
    if (input.bytes_) {
        const auto count =
                static_cast<int>(std::min<std::int64_t>(size, input.size_ - input.position_));
        if (!count) return AVERROR_EOF;
        std::memcpy(buffer, input.bytes_->data() + input.position_,
                    static_cast<std::size_t>(count));
        input.position_ += count;
        return count;
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
    if (input.bytes_ || input.file_bytes_.Valid()) {
        const auto base = whence == SEEK_SET   ? 0
                          : whence == SEEK_CUR ? input.position_
                                               : input.size_;
        if (offset < -base || offset > input.size_ - base) return AVERROR(EINVAL);
        input.position_ = base + offset;
        return input.position_;
    }
    input.file_.clear();
    input.file_.seekg(offset, direction);
    if (!input.file_) {
        return AVERROR(EIO);
    }
    return static_cast<std::int64_t>(input.file_.tellg());
}
}  // namespace rhythm::media::detail
