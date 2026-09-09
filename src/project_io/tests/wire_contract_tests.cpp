#include <iostream>
#include <stdexcept>

#include "wire_limits.h"

namespace {
std::string Varint(std::uint64_t value) {
    std::string bytes;
    do {
        auto next = static_cast<unsigned char>(value & 127);
        value >>= 7;
        if (value) next |= 128;
        bytes.push_back(static_cast<char>(next));
    } while (value);
    return bytes;
}
std::string Message(unsigned field, std::string_view value) {
    return Varint((field << 3) | 2) + Varint(value.size()) + std::string(value);
}
void Reject(std::string_view bytes, rhythm::project::detail::WireRoot root,
            std::string_view expected) {
    try {
        rhythm::project::detail::CheckWireLimits(bytes, root);
    } catch (const std::exception& error) {
        if (error.what() == expected) return;
        throw;
    }
    throw std::runtime_error("wire.invalid_accepted");
}
}  // namespace
int main() {
    using namespace rhythm::project::detail;
    try {
        std::string graph;
        std::string actions;
        for (int index = 0; index < 4096; ++index) actions += Message(1, "");
        const auto track_node = [&](const std::string& records) {
            return Message(4, Message(2, Message(6, records)));
        };
        CheckWireLimits(Message(4, track_node(actions)), WireRoot::kGraph);
        CheckWireLimits(Message(2, Message(4, track_node(actions))), WireRoot::kProgram);
        actions += Message(1, "");
        Reject(Message(4, track_node(actions)), WireRoot::kGraph, "codec.repeated_limit");
        Reject(Message(2, Message(4, track_node(actions))), WireRoot::kProgram,
               "codec.repeated_limit");
        std::string controls;
        for (int i = 0; i < 65; ++i) controls += Message(1, "");
        Reject(Message(11, controls), WireRoot::kGraph, "codec.repeated_limit");
        Reject(Message(7, controls), WireRoot::kProgram, "codec.repeated_limit");
        std::string cues;
        for (int i = 0; i < 257; ++i) cues += Message(3, "");
        Reject(Message(11, cues), WireRoot::kGraph, "codec.repeated_limit");
        Reject(Message(7, cues), WireRoot::kProgram, "codec.repeated_limit");
        Reject(Message(11, Message(3, Message(2, std::string(129, 'a')))), WireRoot::kGraph,
               "codec.string_limit");
        Reject(Message(11, Message(2, Message(2, std::string(129, 'a')))), WireRoot::kGraph,
               "codec.string_limit");
        for (int index = 0; index < 10000; ++index) graph += Message(4, "");
        CheckWireLimits(graph, WireRoot::kGraph);
        Reject(graph + Message(4, ""), WireRoot::kGraph, "codec.repeated_limit");
        std::string keys;
        std::string components;
        for (int index = 0; index < 256; ++index) components += Message(10, "");
        CheckWireLimits(components, WireRoot::kGraph);
        Reject(components + Message(10, ""), WireRoot::kGraph, "codec.repeated_limit");
        Reject(Message(10, Message(9, Message(4, std::string(129, 'x')))), WireRoot::kGraph,
               "codec.string_limit");
        std::string signals;
        for (int index = 0; index < 256; ++index) signals += Message(8, "");
        CheckWireLimits(signals, WireRoot::kGraph);
        Reject(signals + Message(8, ""), WireRoot::kGraph, "codec.repeated_limit");
        Reject(Message(8, Message(1, std::string(129, 'x'))), WireRoot::kGraph,
               "codec.string_limit");
        Reject(Message(9, Message(3, std::string(129, 'x'))), WireRoot::kGraph,
               "codec.string_limit");
        for (int index = 0; index < 1025; ++index) keys += Message(1, "");
        Reject(Message(4, Message(4, Message(2, Message(3, keys)))), WireRoot::kGraph,
               "codec.repeated_limit");
        Reject(Message(2, Message(3, std::string(129, '\0'))), WireRoot::kProgram,
               "codec.slot_limit");
        std::string unpacked;
        for (int index = 0; index < 129; ++index) unpacked += "\x18\x01";
        Reject(Message(2, unpacked), WireRoot::kProgram, "codec.slot_limit");
        Reject(Message(2, Message(3, std::string(128, '\0')) + "\x18\x01"), WireRoot::kProgram,
               "codec.slot_limit");
        Reject(Message(2, Message(4, "") + Message(4, "")), WireRoot::kProgram,
               "codec.repeated_limit");
        Reject(Message(4, Message(2, std::string(257, 'x'))), WireRoot::kGraph,
               "codec.string_limit");
        Reject(Message(4, Message(4, Message(2, Message(4, std::string(1025, 'a'))))),
               WireRoot::kGraph, "codec.string_limit");
        // Unknown groups remain forward compatible, but cannot hide unbounded
        // nesting or bookkeeping allocations from the preflight.
        CheckWireLimits("\x3b\x08\x01\x3c", WireRoot::kGraph);
        Reject("\x3b\x44", WireRoot::kGraph, "codec.group");
        Reject(std::string(34, '\x3b') + std::string(34, '\x3c'), WireRoot::kGraph, "codec.depth");
        Reject(std::string(10, '\xff'), WireRoot::kGraph, "codec.varint");
        Reject("\x22\x05x", WireRoot::kGraph, "codec.truncated");
        std::string fields;
        for (int index = 0; index < 200001; ++index) fields += "\x78\x01";
        Reject(fields, WireRoot::kGraph, "codec.field_budget");
        std::cout << "wire contracts passed: preallocation limits, packed slots, nested curves, "
                     "unknown groups\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
