#pragma once

#include <string_view>

namespace rhythm::project::detail {
enum class WireRoot { kGraph, kProgram };
// Allocation-free schema preflight before generated Protobuf parsing. Rejects
// oversized repeated fields and bounds unknown-field bookkeeping as well.
void CheckWireLimits(std::string_view bytes, WireRoot root);
}  // namespace rhythm::project::detail
