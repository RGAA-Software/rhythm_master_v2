#pragma once

#include <box2d/box2d.h>

#include <utility>

namespace rhythm::physics::detail {
// Native ownership stays in this adapter. World owns its body/joint records;
// declaration order destroys joints, bodies and finally the native world.
template <typename Handle, auto Destroy, auto Valid>
class NativeOwner final {
   public:
    NativeOwner() = default;
    explicit NativeOwner(Handle handle) : handle_(handle) {}
    ~NativeOwner() { Reset(); }
    NativeOwner(NativeOwner&& other) noexcept : handle_(std::exchange(other.handle_, {})) {}
    NativeOwner& operator=(NativeOwner&& other) noexcept {
        if (this != &other) {
            Reset();
            handle_ = std::exchange(other.handle_, {});
        }
        return *this;
    }
    NativeOwner(const NativeOwner&) = delete;
    NativeOwner& operator=(const NativeOwner&) = delete;
    Handle Get() const { return handle_; }

   private:
    void Reset() {
        if (Valid(handle_)) Destroy(handle_);
        handle_ = {};
    }
    Handle handle_{};
};
using NativeWorld = NativeOwner<b2WorldId, b2DestroyWorld, b2World_IsValid>;
using NativeBody = NativeOwner<b2BodyId, b2DestroyBody, b2Body_IsValid>;
using NativeJoint = NativeOwner<b2JointId, b2DestroyJoint, b2Joint_IsValid>;
}  // namespace rhythm::physics::detail
