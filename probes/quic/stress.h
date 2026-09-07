#pragma once

#include <span>

#include "native_api.h"

namespace rhythm::quic_probe {
// Private native candidate adapter test; no application/runtime interface.
void StressConnections(std::shared_ptr<Api> api, const Handle& registration,
                       const Handle& server_config, const Handle& client_config,
                       std::span<const std::uint8_t> certificate, const QUIC_BUFFER& alpn,
                       std::size_t count);
}  // namespace rhythm::quic_probe
