#include <algorithm>
#include <charconv>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <thread>

#include "rhythm/transport/transport.h"

namespace {
using rhythm::cluster::QueueResult;
using rhythm::cluster::SendChannel;
using rhythm::cluster::SendPayload;
using rhythm::transport::EventKind;
using rhythm::transport::Transport;
using namespace std::chrono_literals;
void Require(bool condition) {
    if (!condition) throw std::runtime_error("transport.lan_contract");
}
unsigned Parse(std::string_view value, int base = 10) {
    unsigned result = 0;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result, base);
    Require(parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size());
    return result;
}
rhythm::cluster_auth::Endpoint Address(std::string_view text, unsigned port) {
    Require(port <= 65535);
    rhythm::cluster_auth::Endpoint result;
    result.port_ = static_cast<std::uint16_t>(port);
    for (std::size_t index = 0; index < 4; ++index) {
        const auto split = text.find('.');
        Require((index == 3) == (split == std::string_view::npos));
        const auto byte = Parse(text.substr(0, split));
        Require(byte <= 255);
        result.address_[index] = static_cast<std::uint8_t>(byte);
        if (index != 3) text.remove_prefix(split + 1);
    }
    return result;
}
std::string Hex(const rhythm::security::CertificatePin& pin) {
    constexpr std::string_view kDigits = "0123456789abcdef";
    std::string result;
    for (const auto byte : pin) {
        result += kDigits[byte >> 4];
        result += kDigits[byte & 15];
    }
    return result;
}
rhythm::security::CertificatePin Pin(std::string_view text) {
    rhythm::security::CertificatePin pin{};
    Require(text.size() == 64);
    for (std::size_t index = 0; index < pin.size(); ++index)
        pin[index] = static_cast<std::uint8_t>(Parse(text.substr(index * 2, 2), 16));
    return pin;
}
SendPayload Payload(std::size_t size, std::uint8_t value) {
    return SendPayload(std::vector<std::uint8_t>(size, value));
}
void Exchange(Transport& transport, bool server, rhythm::transport::ConnectionHandle connection) {
    bool connected = false;
    bool datagrams = false;
    bool control = false;
    bool realtime = false;
    bool acknowledged = false;
    bool ack_sent = false;
    std::size_t chunks = 0;
    const auto deadline = std::chrono::steady_clock::now() + 15s;
    auto next_datagram = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() < deadline) {
        auto events = transport.Poll();
        Require(events.size() <= 128);
        for (const auto& event : events) {
            const auto bytes = event.payload_.Bytes();
            switch (event.kind_) {
                case EventKind::kConnected:
                    Require(!connected);
                    connected = true;
                    connection = event.connection_;
                    if (!server)
                        Require(transport.Send(connection, SendChannel::kControl, Payload(1, 9)) ==
                                QueueResult::kAccepted);
                    break;
                case EventKind::kCapabilities:
                    datagrams = event.maximum_datagram_bytes_ >= 100;
                    break;
                case EventKind::kControl:
                    Require(bytes.size() == 1);
                    if (server) {
                        if (bytes[0] == 9)
                            control = true;
                        else {
                            Require(bytes[0] == 10);
                            acknowledged = true;
                        }
                    } else {
                        if (bytes[0] == 8)
                            control = true;
                        else {
                            Require(bytes[0] == 11);
                            acknowledged = true;
                        }
                    }
                    break;
                case EventKind::kAsset:
                    Require(!server && bytes.size() == 65536 && chunks < 16);
                    Require(std::all_of(bytes.begin(), bytes.end(),
                                        [&](auto value) { return value == chunks; }));
                    ++chunks;
                    break;
                case EventKind::kRealtime:
                    Require(!server && bytes.size() == 100 &&
                            std::all_of(bytes.begin(), bytes.end(),
                                        [](auto value) { return value == 17; }));
                    realtime = true;
                    break;
                case EventKind::kDisconnected:
                    Require(server && acknowledged && ack_sent && chunks == 16);
                    std::cout << "Public transport LAN server passed: 1 MiB assets, control, "
                                 "DATAGRAM and close\n";
                    return;
            }
        }
        if (server && connected && control) {
            if (!chunks)
                Require(transport.Send(connection, SendChannel::kControl, Payload(1, 8)) ==
                        QueueResult::kAccepted);
            while (chunks < 16) {
                const auto result =
                        transport.Send(connection, SendChannel::kAsset,
                                       Payload(65536, static_cast<std::uint8_t>(chunks)));
                if (result == QueueResult::kFull) break;
                Require(result == QueueResult::kAccepted);
                ++chunks;
            }
            if (datagrams && !acknowledged && std::chrono::steady_clock::now() >= next_datagram) {
                const auto result =
                        transport.Send(connection, SendChannel::kRealtime, Payload(100, 17));
                Require(result == QueueResult::kAccepted || result == QueueResult::kReplaced);
                next_datagram = std::chrono::steady_clock::now() + 50ms;
            }
            if (acknowledged && !ack_sent) {
                Require(transport.Send(connection, SendChannel::kControl, Payload(1, 11)) ==
                        QueueResult::kAccepted);
                ack_sent = true;
            }
        }
        if (!server && control && realtime && chunks == 16) {
            if (!ack_sent) {
                Require(transport.Send(connection, SendChannel::kControl, Payload(1, 10)) ==
                        QueueResult::kAccepted);
                ack_sent = true;
            }
            if (acknowledged) {
                transport.Disconnect(connection);
                std::cout << "Public transport LAN client passed: exact asset bytes/order and "
                             "simultaneous channels\n";
                return;
            }
        }
        std::this_thread::sleep_for(1ms);
    }
    throw std::runtime_error("transport.lan_timeout");
}
}  // namespace
int TransportLan(int argc, char** argv) {
    Require(argc == 3 || argc == 5);
    const bool server = std::string_view(argv[1]) == "server";
    Require(server ? argc == 3 : argc == 5 && std::string_view(argv[1]) == "client");
    if (server) {
        auto identity = rhythm::security::HostIdentity::Create();
        auto transport = Transport::Listen(identity, Address(argv[2], 0), 1);
        // Certificate fingerprint is public; no invitation or private key is logged.
        std::cout << "READY " << transport.LocalEndpoint()->port_ << ' '
                  << Hex(identity.Fingerprint()) << std::endl;
        Exchange(transport, true, {});
    } else {
        auto transport = Transport::Client();
        const auto connection = transport.Connect(Address(argv[2], Parse(argv[3])), Pin(argv[4]));
        Require(connection.has_value());
        Exchange(transport, false, *connection);
    }
    return 0;
}
