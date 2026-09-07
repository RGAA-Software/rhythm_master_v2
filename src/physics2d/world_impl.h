#pragma once

#include <map>

#include "native_handles.h"
#include "rhythm/physics/world.h"

namespace rhythm::physics {
namespace detail {
bool Bounded(float value, float minimum, float maximum);
bool Bounded(Vector vector, float maximum = 10000);
b2Vec2 Native(Vector vector);
Vector Value(b2Vec2 vector);
void Require(bool condition, const char* message = "physics.arguments");
}  // namespace detail
class World::Impl final {
   public:
    explicit Impl(WorldConfig config);
    struct BodyRecord {
        detail::NativeBody native_{};
        b2ShapeId shape_{};  // Borrowed from native_; never leaves the adapter.
    };
    struct JointRecord {
        detail::NativeJoint native_{};
        BodyHandle first_{};
        BodyHandle second_{};
    };
    b2BodyId Body(BodyHandle handle) const;
    void ReadContacts(StepResult& result);
    WorldConfig config_{};
    std::uint64_t identity_ = 0;
    std::uint64_t next_body_ = 1;
    std::uint64_t next_joint_ = 1;
    double accumulator_ = 0;
    detail::NativeWorld native_{};
    std::map<std::uint64_t, BodyRecord> bodies_{};
    std::map<std::uint64_t, BodyHandle> shape_bodies_{};
    std::map<std::uint64_t, JointRecord> joints_{};
};
}  // namespace rhythm::physics
