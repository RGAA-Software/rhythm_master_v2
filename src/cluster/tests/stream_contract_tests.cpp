#include <iostream>
#include <stdexcept>

#include "rhythm/cluster/stream_decoder.h"

int main() {
    using namespace rhythm::cluster;
    try {
        const auto check = [](bool value) {
            if (!value) throw std::runtime_error("cluster.stream_contract");
        };
        const std::array<std::uint8_t, 7> wire{0, 0, 0, 3, 7, 8, 9};
        for (std::size_t split = 0; split <= wire.size(); ++split) {
            StreamDecoder decoder(SendChannel::kControl);
            const auto bytes = std::span(wire);
            check(decoder.Feed(bytes.first(split)).consumed_ == split);
            check(decoder.Feed(bytes.subspan(split)).consumed_ == wire.size() - split);
            check(decoder.Finish());
            const auto message = decoder.Take().value();
            check(message.Bytes().size() == 3 && message.Bytes()[0] == 7 &&
                  message.Bytes()[2] == 9);
            check(!decoder.Take() && decoder.Feed(wire).status_ == StreamFeedStatus::kClosed);
        }
        for (std::size_t truncated = 1; truncated < wire.size(); ++truncated) {
            StreamDecoder decoder(SendChannel::kControl);
            decoder.Feed(std::span(wire).first(truncated));
            check(!decoder.Finish());
        }
        for (const auto length : {0U, 4097U, 0xffffffffU}) {
            StreamDecoder decoder(SendChannel::kControl);
            const std::array<std::uint8_t, 4> header{static_cast<std::uint8_t>(length >> 24),
                                                     static_cast<std::uint8_t>(length >> 16),
                                                     static_cast<std::uint8_t>(length >> 8),
                                                     static_cast<std::uint8_t>(length)};
            check(decoder.Feed(header).status_ == StreamFeedStatus::kInvalid);
            check(decoder.QueuedBytes() == 0 && !decoder.Take());
        }
        StreamDecoder control(SendChannel::kControl);
        std::vector<std::uint8_t> batch;
        for (int index = 0; index < 1000; ++index)
            batch.insert(batch.end(), wire.begin(), wire.end());
        std::size_t offset = 0;
        std::size_t messages = 0;
        while (offset < batch.size()) {
            const auto result = control.Feed(std::span(batch).subspan(offset));
            check(result.status_ == StreamFeedStatus::kConsumed ||
                  result.status_ == StreamFeedStatus::kBackpressure);
            offset += result.consumed_;
            check(control.QueuedCount() <= 64 && control.QueuedBytes() <= 65536);
            while (control.Take()) ++messages;
        }
        check(messages == 1000 && control.Finish());
        StreamDecoder assets(SendChannel::kAsset);
        std::vector<std::uint8_t> block(65540, 42);
        block[0] = 0;
        block[1] = 1;
        block[2] = 0;
        block[3] = 0;
        for (int index = 0; index < 4; ++index) check(assets.Feed(block).consumed_ == block.size());
        const auto blocked = assets.Feed(block);
        check(blocked.status_ == StreamFeedStatus::kBackpressure && blocked.consumed_ == 4);
        check(assets.QueuedBytes() == 262144 && assets.Take()->Bytes().size() == 65536);
        check(assets.Feed(std::span(block).subspan(blocked.consumed_)).consumed_ == 65536);
        check(assets.QueuedBytes() == 262144);
        assets.Cancel();
        check(!assets.Take() && assets.QueuedBytes() == 0 &&
              assets.Feed(block).status_ == StreamFeedStatus::kClosed);
        std::cout << "stream contracts passed: fragmentation, framing limits, FIN truncation, "
                     "coalesced packets, bounded backpressure\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
