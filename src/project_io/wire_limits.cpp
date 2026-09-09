#include "wire_limits.h"

#include <array>
#include <cstdint>
#include <optional>
#include <stdexcept>

namespace rhythm::project::detail {
namespace {
enum class Kind {
    kGraph,
    kProgram,
    kNode,
    kEdge,
    kInstruction,
    kMap,
    kProperty,
    kCurve,
    kKey,
    kColor,
    kCanvas,
    kNamedSignal,
    kSignalBinding,
    kComponent,
    kComponentInput,
    kComponentParameter,
    kControls,
    kControlTitle,
    kControlSnapshot,
    kControlValue,
    kControlCue,
    kBeatGrid
};
struct Budget {
    std::size_t fields_ = 0;
    std::size_t messages_ = 0;
};
class Cursor final {
   public:
    explicit Cursor(std::string_view bytes) : bytes_(bytes) {}
    bool Empty() const { return bytes_.empty(); }
    std::uint64_t Varint() {
        std::uint64_t result = 0;
        for (unsigned shift = 0; shift < 70; shift += 7) {
            if (bytes_.empty()) throw std::invalid_argument("codec.truncated");
            const auto value = static_cast<std::uint8_t>(bytes_.front());
            bytes_.remove_prefix(1);
            if (shift == 63 && value > 1) throw std::invalid_argument("codec.varint");
            result |= std::uint64_t(value & 127) << shift;
            if (!(value & 128)) return result;
        }
        throw std::invalid_argument("codec.varint");
    }
    std::string_view Take(std::uint64_t length) {
        if (length > bytes_.size()) throw std::invalid_argument("codec.truncated");
        const auto result = bytes_.substr(0, static_cast<std::size_t>(length));
        bytes_.remove_prefix(static_cast<std::size_t>(length));
        return result;
    }

   private:
    std::string_view bytes_{};
};
std::uint32_t Tag(Cursor& cursor, Budget& budget) {
    if (++budget.fields_ > 200000) throw std::length_error("codec.field_budget");
    const auto tag = cursor.Varint();
    if (tag > 0xffffffff || (tag >> 3) == 0) throw std::invalid_argument("codec.tag");
    return static_cast<std::uint32_t>(tag);
}
void Skip(Cursor& cursor, std::uint32_t tag, unsigned depth, Budget& budget) {
    if (depth > 32) throw std::length_error("codec.depth");
    switch (tag & 7) {
        case 0:
            cursor.Varint();
            return;
        case 1:
            cursor.Take(8);
            return;
        case 2:
            cursor.Take(cursor.Varint());
            return;
        case 3:
            while (!cursor.Empty()) {
                const auto nested = Tag(cursor, budget);
                if ((nested & 7) == 4) {
                    if ((nested >> 3) != (tag >> 3)) throw std::invalid_argument("codec.group");
                    return;
                }
                Skip(cursor, nested, depth + 1, budget);
            }
            throw std::invalid_argument("codec.group");
        case 5:
            cursor.Take(4);
            return;
        default:
            throw std::invalid_argument("codec.wire_type");
    }
}
struct Child {
    Kind kind_ = Kind::kNode;
    std::size_t maximum_ = 1;
};
std::optional<Child> Nested(Kind kind, unsigned field) {
    if ((kind == Kind::kGraph && field == 12) || (kind == Kind::kProgram && field == 8))
        return Child{Kind::kBeatGrid};
    if ((kind == Kind::kGraph && field == 11) || (kind == Kind::kProgram && field == 7))
        return Child{Kind::kControls};
    if (kind == Kind::kControls && field == 1) return Child{Kind::kControlTitle, 64};
    if (kind == Kind::kControls && field == 2) return Child{Kind::kControlSnapshot, 64};
    if (kind == Kind::kControls && field == 3) return Child{Kind::kControlCue, 256};
    if (kind == Kind::kControlSnapshot && field == 3) return Child{Kind::kControlValue, 64};
    if ((kind == Kind::kGraph && field == 7) || (kind == Kind::kProgram && field == 6))
        return Child{Kind::kCanvas};
    if (kind == Kind::kGraph && field == 4) return Child{Kind::kNode, 10000};
    if (kind == Kind::kGraph && field == 5) return Child{Kind::kEdge, 40000};
    if (kind == Kind::kGraph && field == 8) return Child{Kind::kNamedSignal, 256};
    if (kind == Kind::kGraph && field == 9) return Child{Kind::kSignalBinding, 4096};
    if (kind == Kind::kGraph && field == 10) return Child{Kind::kComponent, 256};
    if (kind == Kind::kComponent && field == 3) return Child{Kind::kNode, 10000};
    if (kind == Kind::kComponent && field == 4) return Child{Kind::kEdge, 40000};
    if (kind == Kind::kComponent && field == 6) return Child{Kind::kNamedSignal, 256};
    if (kind == Kind::kComponent && field == 7) return Child{Kind::kSignalBinding, 4096};
    if (kind == Kind::kComponent && field == 8) return Child{Kind::kComponentInput, 128};
    if (kind == Kind::kComponent && field == 9) return Child{Kind::kComponentParameter, 128};
    if (kind == Kind::kProgram && field == 2) return Child{Kind::kInstruction, 10000};
    if (kind == Kind::kInstruction && field == 4) return Child{Kind::kNode};
    if (kind == Kind::kNode && field == 4) return Child{Kind::kMap, 128};
    if (kind == Kind::kMap && field == 2) return Child{Kind::kProperty};
    if (kind == Kind::kProperty && field == 2) return Child{Kind::kColor};
    if (kind == Kind::kProperty && field == 3) return Child{Kind::kCurve};
    if (kind == Kind::kCurve && field == 1) return Child{Kind::kKey, 1024};
    return std::nullopt;
}
std::size_t StringLimit(Kind kind, unsigned field) {
    if ((kind == Kind::kControlTitle || kind == Kind::kControlSnapshot ||
         kind == Kind::kControlCue) &&
        field == 2)
        return 128;
    if (kind == Kind::kComponent && (field == 1 || field == 10)) return 256;
    if ((kind == Kind::kComponentInput && (field == 1 || field == 3)) ||
        (kind == Kind::kComponentParameter && (field == 1 || field == 3 || field == 4)))
        return 128;
    if ((kind == Kind::kNamedSignal && field == 1) ||
        (kind == Kind::kSignalBinding && (field == 2 || field == 3)))
        return 128;
    if (kind == Kind::kProperty && field == 4) return 1024;
    if (kind == Kind::kProperty && field == 5) return 64;
    if ((kind == Kind::kGraph && field == 2) || (kind == Kind::kProgram && field == 4) ||
        (kind == Kind::kNode && field == 2) || (kind == Kind::kInstruction && field == 1))
        return 256;
    if ((kind == Kind::kMap && field == 1) || (kind == Kind::kEdge && field == 4)) return 128;
    return 8 * 1024 * 1024;
}
void Scan(std::string_view bytes, Kind kind, unsigned depth, Budget& budget) {
    if (depth > 32 || ++budget.messages_ > 100000) throw std::length_error("codec.message_budget");
    Cursor cursor(bytes);
    std::array<std::size_t, 13> counts{};
    std::size_t slots = 0;
    while (!cursor.Empty()) {
        const auto tag = Tag(cursor, budget);
        const auto field = tag >> 3;
        const auto wire = tag & 7;
        if (kind == Kind::kInstruction && field == 3 && wire == 0) {
            if (++slots > 128) throw std::length_error("codec.slot_limit");
            cursor.Varint();
        } else if (wire == 2) {
            const auto value = cursor.Take(cursor.Varint());
            if (value.size() > StringLimit(kind, field))
                throw std::length_error("codec.string_limit");
            if (const auto child = Nested(kind, field)) {
                if (++counts.at(field) > child->maximum_)
                    throw std::length_error("codec.repeated_limit");
                Scan(value, child->kind_, depth + 1, budget);
            } else if (kind == Kind::kInstruction && field == 3) {
                Cursor packed(value);
                while (!packed.Empty()) {
                    if (++slots > 128) throw std::length_error("codec.slot_limit");
                    packed.Varint();
                }
            }
        } else {
            Skip(cursor, tag, depth, budget);
        }
    }
}
}  // namespace
void CheckWireLimits(std::string_view bytes, WireRoot root) {
    Budget budget;
    Scan(bytes, root == WireRoot::kGraph ? Kind::kGraph : Kind::kProgram, 0, budget);
}
}  // namespace rhythm::project::detail
