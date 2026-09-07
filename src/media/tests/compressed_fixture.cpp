#include "compressed_fixture.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "../ffmpeg_resources.h"

namespace rhythm::media::test {
namespace {
struct OutputDelete {
    void operator()(AVFormatContext* value) const {
        if (value->pb) {
            avio_closep(&value->pb);
        }
        avformat_free_context(value);
    }
};
}  // namespace
void WriteFlac(const std::filesystem::path& path) {
    AVFormatContext* created = nullptr;
    const int allocation = avformat_alloc_output_context2(&created, nullptr, "flac", nullptr);
    std::unique_ptr<AVFormatContext, OutputDelete> output(created);
    detail::Check(allocation, "create FLAC fixture");
    const auto* encoder = avcodec_find_encoder(AV_CODEC_ID_FLAC);
    if (!encoder || !output) {
        throw std::runtime_error("FLAC fixture encoder unavailable");
    }
    std::unique_ptr<AVCodecContext, detail::CodecDelete> codec(avcodec_alloc_context3(encoder));
    std::unique_ptr<AVFrame, detail::FrameDelete> frame(av_frame_alloc());
    std::unique_ptr<AVPacket, detail::PacketDelete> packet(av_packet_alloc());
    if (!codec || !frame || !packet) {
        throw std::bad_alloc();
    }
    codec->sample_rate = 44100;
    codec->sample_fmt = AV_SAMPLE_FMT_S16;
    av_channel_layout_default(&codec->ch_layout, 2);
    codec->time_base = AVRational{1, 44100};
    if (output->oformat->flags & AVFMT_GLOBALHEADER) {
        codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
    detail::Check(avcodec_open2(codec.get(), encoder, nullptr), "open FLAC encoder");
    auto* stream = avformat_new_stream(output.get(), nullptr);
    if (!stream) {
        throw std::bad_alloc();
    }
    stream->time_base = codec->time_base;
    detail::Check(avcodec_parameters_from_context(stream->codecpar, codec.get()),
                  "FLAC stream parameters");
    const auto filename = path.u8string();
    detail::Check(avio_open(&output->pb, reinterpret_cast<const char*>(filename.c_str()),
                            AVIO_FLAG_WRITE),
                  "open FLAC output");
    detail::Check(avformat_write_header(output.get(), nullptr), "write FLAC header");
    frame->format = codec->sample_fmt;
    frame->sample_rate = codec->sample_rate;
    frame->nb_samples = codec->frame_size;
    detail::Check(av_channel_layout_copy(&frame->ch_layout, &codec->ch_layout),
                  "FLAC frame channels");
    detail::Check(av_frame_get_buffer(frame.get(), 0), "FLAC frame buffer");
    const auto drain = [&] {
        for (;;) {
            const int result = avcodec_receive_packet(codec.get(), packet.get());
            if (result == AVERROR(EAGAIN) || result == AVERROR_EOF) {
                break;
            }
            detail::Check(result, "receive FLAC packet");
            av_packet_rescale_ts(packet.get(), codec->time_base, stream->time_base);
            packet->stream_index = stream->index;
            detail::Check(av_interleaved_write_frame(output.get(), packet.get()),
                          "write FLAC packet");
            av_packet_unref(packet.get());
        }
    };
    for (int position = 0; position < 44100;) {
        detail::Check(av_frame_make_writable(frame.get()), "write FLAC samples");
        frame->nb_samples = std::min(codec->frame_size, 44100 - position);
        frame->pts = position;
        auto* samples = reinterpret_cast<std::int16_t*>(frame->data[0]);
        for (int index = 0; index < frame->nb_samples; ++index) {
            const auto sample = static_cast<std::int16_t>(
                    12000 * std::sin(2 * std::numbers::pi * 440 * (position + index) / 44100));
            samples[index * 2] = sample;
            samples[index * 2 + 1] = static_cast<std::int16_t>(-sample);
        }
        detail::Check(avcodec_send_frame(codec.get(), frame.get()), "send FLAC samples");
        drain();
        position += frame->nb_samples;
    }
    detail::Check(avcodec_send_frame(codec.get(), nullptr), "flush FLAC encoder");
    drain();
    detail::Check(av_write_trailer(output.get()), "finalize FLAC fixture");
}
}  // namespace rhythm::media::test
