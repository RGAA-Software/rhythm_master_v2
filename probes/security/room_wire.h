#pragma once

#include "connection.h"

namespace rhythm::security::probe {
void RoomWire(rhythm::quic_probe::Connection& client, rhythm::quic_probe::Connection& server,
              bool wrong_secret, bool revoked);
}  // namespace rhythm::security::probe
