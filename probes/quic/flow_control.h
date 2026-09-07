#pragma once

#include "connection.h"

namespace rhythm::quic_probe {
void FlowControl(Connection& client, Connection& server, bool cancel_paused);
}  // namespace rhythm::quic_probe
