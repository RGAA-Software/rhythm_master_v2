// Bounded in-memory C API experiment; does not implement a socket transport.
#include <quiche.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>
#ifndef _WIN32
#include <netinet/in.h>
#endif

namespace {
void Require(bool value, const std::string& error) {
    if (!value) throw std::runtime_error(error);
}
struct ConfigCloser {
    void operator()(quiche_config* value) const { quiche_config_free(value); }
};
struct ConnectionCloser {
    void operator()(quiche_conn* value) const { quiche_conn_free(value); }
};
using Config = std::unique_ptr<quiche_config, ConfigCloser>;
using Connection = std::unique_ptr<quiche_conn, ConnectionCloser>;
Config Configure() {
    Config config(quiche_config_new(QUICHE_PROTOCOL_VERSION));
    Require(static_cast<bool>(config), "configuration.allocate");
    const std::array<std::uint8_t, 16> alpn{15,  'r', 'h', 'y', 't', 'h', 'm', '-',
                                            'p', 'r', 'o', 'b', 'e', '-', 'v', '1'};
    Require(quiche_config_set_application_protos(config.get(), alpn.data(), alpn.size()) == 0,
            "configuration.alpn");
    quiche_config_set_max_idle_timeout(config.get(), 5000);
    quiche_config_set_max_recv_udp_payload_size(config.get(), 1350);
    quiche_config_set_max_send_udp_payload_size(config.get(), 1350);
    quiche_config_set_initial_max_data(config.get(), 4096);
    quiche_config_set_initial_max_stream_data_uni(config.get(), 1024);
    quiche_config_set_initial_max_streams_uni(config.get(), 1);
    quiche_config_enable_dgram(config.get(), true, 2, 2);
    // This adapter moves bounded packets directly between two engines; a real
    // socket transport must implement pacing rather than use this test setting.
    quiche_config_enable_pacing(config.get(), false);
    return config;
}
void Scenario(const std::filesystem::path& fixtures, bool correct_ca) {
    auto server_config = Configure();
    auto client_config = Configure();
    Require(quiche_config_load_cert_chain_from_pem_file(
                    server_config.get(), (fixtures / "server.pem").string().c_str()) == 0,
            "credentials.certificate");
    Require(quiche_config_load_priv_key_from_pem_file(
                    server_config.get(), (fixtures / "server.key").string().c_str()) == 0,
            "credentials.key");
    Require(quiche_config_load_verify_locations_from_file(
                    client_config.get(),
                    (fixtures / (correct_ca ? "server.pem" : "other.pem")).string().c_str()) == 0,
            "credentials.trust");
    quiche_config_verify_peer(client_config.get(), true);
    sockaddr_in client_address{};
    client_address.sin_family = AF_INET;
    client_address.sin_port = htons(40401);
    client_address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    auto server_address = client_address;
    server_address.sin_port = htons(40402);
    const std::array<std::uint8_t, 16> client_id{7, 9, 13, 17};
    Connection client(quiche_connect("localhost", client_id.data(), client_id.size(),
                                     reinterpret_cast<const sockaddr*>(&client_address),
                                     sizeof(client_address),
                                     reinterpret_cast<const sockaddr*>(&server_address),
                                     sizeof(server_address), client_config.get()));
    Require(static_cast<bool>(client), "client.create");
    Connection server;
    bool tls_rejected = false;
    auto pump = [&](bool from_client) {
        auto& sender = from_client ? client : server;
        auto& receiver = from_client ? server : client;
        if (!sender) return;
        for (int count = 0; count < 64; ++count) {
            std::array<std::uint8_t, 1350> packet{};
            quiche_send_info sent{};
            const auto length = quiche_conn_send(sender.get(), packet.data(), packet.size(), &sent);
            if (length == QUICHE_ERR_DONE) break;
            Require(length > 0, "packet.send:" + std::to_string(length));
            if (!receiver) {
                std::array<std::uint8_t, QUICHE_MAX_CONN_ID_LEN> source_id{};
                std::array<std::uint8_t, QUICHE_MAX_CONN_ID_LEN> destination_id{};
                std::array<std::uint8_t, 512> token{};
                std::size_t source_size = source_id.size();
                std::size_t destination_size = destination_id.size();
                std::size_t token_size = token.size();
                std::uint32_t version = 0;
                std::uint8_t type = 0;
                Require(quiche_header_info(packet.data(), static_cast<std::size_t>(length),
                                           client_id.size(), &version, &type, source_id.data(),
                                           &source_size, destination_id.data(), &destination_size,
                                           token.data(), &token_size) == 0,
                        "packet.header");
                server.reset(quiche_accept(destination_id.data(), destination_size, nullptr, 0,
                                           reinterpret_cast<const sockaddr*>(&server_address),
                                           sizeof(server_address),
                                           reinterpret_cast<const sockaddr*>(&client_address),
                                           sizeof(client_address), server_config.get()));
                Require(static_cast<bool>(server), "server.accept");
            }
            quiche_recv_info received{reinterpret_cast<sockaddr*>(&sent.from), sent.from_len,
                                      reinterpret_cast<sockaddr*>(&sent.to), sent.to_len};
            const auto result = quiche_conn_recv(receiver.get(), packet.data(),
                                                 static_cast<std::size_t>(length), &received);
            if (!correct_ca && !from_client &&
                (result == QUICHE_ERR_TLS_FAIL || result == QUICHE_ERR_CRYPTO_FAIL)) {
                tls_rejected = true;
                break;
            }
            Require(result >= 0 || result == QUICHE_ERR_DONE,
                    "packet.receive:" + std::to_string(result));
        }
    };
    auto progress = [&] {
        pump(true);
        pump(false);
        for (const auto& connection : {std::cref(client), std::cref(server)})
            if (connection.get() && quiche_conn_timeout_as_nanos(connection.get().get()) == 0)
                quiche_conn_on_timeout(connection.get().get());
    };
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!tls_rejected &&
           !(server && quiche_conn_is_established(client.get()) &&
             quiche_conn_is_established(server.get())) &&
           std::chrono::steady_clock::now() < deadline) {
        progress();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (!correct_ca) {
        Require(tls_rejected && !quiche_conn_is_established(client.get()), "credentials.reject");
        std::cout << "quiche rejected incorrect CA before established\n";
        return;
    }
    Require(server && quiche_conn_is_established(client.get()) &&
                    quiche_conn_is_established(server.get()),
            "handshake.timeout");
    const std::array<std::uint8_t, 16> payload{1, 3, 5, 7, 9};
    Require(quiche_conn_dgram_send(client.get(), payload.data(), payload.size()) ==
                    static_cast<ssize_t>(payload.size()),
            "datagram.first");
    Require(quiche_conn_dgram_send(client.get(), payload.data(), payload.size()) ==
                    static_cast<ssize_t>(payload.size()),
            "datagram.second");
    Require(quiche_conn_is_dgram_send_queue_full(client.get()) &&
                    quiche_conn_dgram_send_queue_len(client.get()) == 2 &&
                    quiche_conn_dgram_send(client.get(), payload.data(), payload.size()) ==
                            QUICHE_ERR_DONE,
            "datagram.queue_bound");
    std::uint64_t error_code = 0;
    Require(quiche_conn_stream_send(client.get(), 2, payload.data(), payload.size(), true,
                                    &error_code) == static_cast<ssize_t>(payload.size()),
            "stream.send");
    while (std::chrono::steady_clock::now() < deadline &&
           (!quiche_conn_stream_readable(server.get(), 2) ||
            quiche_conn_dgram_recv_queue_len(server.get()) != 2))
        progress();
    std::array<std::uint8_t, 1024> output{};
    bool fin = false;
    Require(quiche_conn_stream_recv(server.get(), 2, output.data(), output.size(), &fin,
                                    &error_code) == static_cast<ssize_t>(payload.size()) &&
                    fin && std::equal(payload.begin(), payload.end(), output.begin()),
            "stream.receive");
    for (int index = 0; index < 2; ++index)
        Require(quiche_conn_dgram_recv(server.get(), output.data(), output.size()) ==
                                static_cast<ssize_t>(payload.size()) &&
                        std::equal(payload.begin(), payload.end(), output.begin()),
                "datagram.receive");
    Require(quiche_conn_close(client.get(), true, 0, nullptr, 0) == 0, "connection.close");
    progress();
    Require(quiche_conn_is_draining(server.get()), "connection.draining");
    std::cout << "quiche C API: verified TLS, bounded DATAGRAM queue, reliable stream and close "
                 "passed\n";
}
}  // namespace
int main(int argc, char* argv[]) {
    try {
        Require(argc == 2, "usage: quiche_probe <fixtures>");
        Scenario(argv[1], true);
        Scenario(argv[1], false);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
