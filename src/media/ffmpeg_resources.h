#pragma once

// Native FFmpeg ownership is confined to this implementation boundary. Each
// unique_ptr owns one library allocation; deleter pointer parameters are borrowed
// solely by the corresponding synchronous C release call.
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
}

#include <array>
#include <memory>
#include <stdexcept>
#include <string>

namespace rhythm::media::detail {
struct FormatDelete {
    void operator()(AVFormatContext* value) const { avformat_close_input(&value); }
};
struct CodecDelete {
    void operator()(AVCodecContext* value) const { avcodec_free_context(&value); }
};
struct PacketDelete {
    void operator()(AVPacket* value) const { av_packet_free(&value); }
};
struct FrameDelete {
    void operator()(AVFrame* value) const { av_frame_free(&value); }
};
struct ResamplerDelete {
    void operator()(SwrContext* value) const { swr_free(&value); }
};
struct IoDelete {
    void operator()(AVIOContext* value) const {
        av_freep(&value->buffer);
        avio_context_free(&value);
    }
};
struct BufferDelete {
    void operator()(unsigned char* value) const { av_free(value); }
};
inline void Check(int result, const std::string& operation) {
    if (result >= 0) {
        return;
    }
    std::array<char, AV_ERROR_MAX_STRING_SIZE> message{};
    av_strerror(result, message.data(), message.size());
    throw std::runtime_error(operation + ": " + message.data());
}
}  // namespace rhythm::media::detail
