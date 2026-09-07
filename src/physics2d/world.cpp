// Focused first-party adapter redesign; provenance/physics2d.json records sources.
#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "world_impl.h"

namespace rhythm::physics {
namespace detail {
bool Bounded(float value, float minimum, float maximum) {
    return std::isfinite(value) && value >= minimum && value <= maximum;
}
bool Bounded(Vector vector, float maximum) {
    return Bounded(vector.x_, -maximum, maximum) && Bounded(vector.y_, -maximum, maximum);
}
b2Vec2 Native(Vector vector) { return {vector.x_, vector.y_}; }
Vector Value(b2Vec2 vector) { return {vector.x, vector.y}; }
void Require(bool condition, const char* message) {
    if (!condition) throw std::invalid_argument(message);
}
}  // namespace detail
namespace {
std::atomic<std::uint64_t> next_world{1};
std::uint64_t NextIdentity() {
    auto value = next_world.load();
    for (;;) {
        detail::Require(value != UINT64_MAX, "physics.id_exhausted");
        if (next_world.compare_exchange_weak(value, value + 1)) return value;
    }
}
detail::NativeWorld CreateNativeWorld(WorldConfig config) {
    detail::Require(detail::Bounded(config.gravity_, 100) && config.body_limit_ > 0 &&
                            config.body_limit_ <= 4096 && config.joint_limit_ <= 4096,
                    "physics.config");
    auto definition = b2DefaultWorldDef();
    definition.gravity = detail::Native(config.gravity_);
    definition.enableSleep = config.sleep_;
    definition.hitEventThreshold = 0.1f;
    auto world = detail::NativeWorld(b2CreateWorld(&definition));
    if (!b2World_IsValid(world.Get())) throw std::runtime_error("physics.create_world");
    return world;
}
}  // namespace
World::Impl::Impl(WorldConfig config)
    : config_(config), identity_(NextIdentity()), native_(CreateNativeWorld(config)) {
    detail::Require(identity_ != 0, "physics.id_exhausted");
}
b2BodyId World::Impl::Body(BodyHandle handle) const {
    const auto found = bodies_.find(handle.id_);
    detail::Require(handle.world_ == identity_ && found != bodies_.end(), "physics.body_handle");
    return found->second.native_.Get();
}
World::World(WorldConfig config) : impl_(std::make_unique<Impl>(config)) {}
World::~World() = default;
World::World(World&&) noexcept = default;
World& World::operator=(World&&) noexcept = default;
BodyHandle World::CreateBody(const BodyDefinition& definition) {
    using detail::Bounded;
    detail::Require(impl_->bodies_.size() < impl_->config_.body_limit_, "physics.body_limit");
    detail::Require(
            Bounded(definition.position_) && Bounded(definition.velocity_, 200) &&
            Bounded(definition.angle_, -10000, 10000) &&
            Bounded(definition.angular_velocity_, -200, 200) &&
            Bounded(definition.radius_, 0.005f, 100) && Bounded(definition.half_extents_, 10000) &&
            definition.half_extents_.x_ >= 0.005f && definition.half_extents_.y_ >= 0.005f &&
            Bounded(definition.density_, 0.001f, 100) && Bounded(definition.friction_, 0, 1) &&
            Bounded(definition.restitution_, 0, 1) && Bounded(definition.gravity_scale_, -10, 10) &&
            (definition.type_ == BodyType::kStatic || definition.type_ == BodyType::kKinematic ||
             definition.type_ == BodyType::kDynamic) &&
            (definition.shape_ == Shape::kCircle || definition.shape_ == Shape::kBox));
    detail::Require(impl_->next_body_ != UINT64_MAX, "physics.id_exhausted");
    auto native = b2DefaultBodyDef();
    native.type = definition.type_ == BodyType::kStatic      ? b2_staticBody
                  : definition.type_ == BodyType::kKinematic ? b2_kinematicBody
                                                             : b2_dynamicBody;
    native.position = detail::Native(definition.position_);
    native.rotation = b2MakeRot(definition.angle_);
    native.linearVelocity = detail::Native(definition.velocity_);
    native.angularVelocity = definition.angular_velocity_;
    native.gravityScale = definition.gravity_scale_;
    native.isBullet = definition.bullet_;
    Impl::BodyRecord record;
    record.native_ = detail::NativeBody(b2CreateBody(impl_->native_.Get(), &native));
    detail::Require(b2Body_IsValid(record.native_.Get()), "physics.create_body");
    auto shape = b2DefaultShapeDef();
    shape.density = definition.density_;
    shape.material.friction = definition.friction_;
    shape.material.restitution = definition.restitution_;
    shape.isSensor = definition.sensor_;
    shape.enableSensorEvents = true;
    shape.enableContactEvents = true;
    shape.enableHitEvents = true;
    shape.filter.categoryBits = definition.category_;
    shape.filter.maskBits = definition.mask_;
    if (definition.shape_ == Shape::kCircle) {
        const b2Circle circle{{0, 0}, definition.radius_};
        record.shape_ = b2CreateCircleShape(record.native_.Get(), &shape, &circle);
    } else {
        const auto box = b2MakeBox(definition.half_extents_.x_, definition.half_extents_.y_);
        record.shape_ = b2CreatePolygonShape(record.native_.Get(), &shape, &box);
    }
    detail::Require(b2Shape_IsValid(record.shape_), "physics.create_shape");
    const BodyHandle handle{impl_->identity_, impl_->next_body_++};
    const auto shape_id = b2StoreShapeId(record.shape_);
    impl_->bodies_.emplace(handle.id_, std::move(record));
    try {
        impl_->shape_bodies_.emplace(shape_id, handle);
    } catch (...) {
        impl_->bodies_.erase(handle.id_);
        throw;
    }
    return handle;
}
bool World::Contains(BodyHandle body) const {
    return body.world_ == impl_->identity_ && impl_->bodies_.contains(body.id_);
}
void World::DestroyBody(BodyHandle body) {
    (void)impl_->Body(body);
    std::erase_if(impl_->joints_, [&](const auto& item) {
        return item.second.first_ == body || item.second.second_ == body;
    });
    impl_->shape_bodies_.erase(b2StoreShapeId(impl_->bodies_.at(body.id_).shape_));
    impl_->bodies_.erase(body.id_);
}
BodyState World::ReadBody(BodyHandle body) const {
    const auto native = impl_->Body(body);
    return {detail::Value(b2Body_GetPosition(native)),
            detail::Value(b2Body_GetLinearVelocity(native)),
            b2Rot_GetAngle(b2Body_GetRotation(native)), b2Body_GetAngularVelocity(native),
            b2Body_IsAwake(native)};
}
void World::SetVelocity(BodyHandle body, Vector velocity, float angular_velocity) {
    detail::Require(detail::Bounded(velocity, 200) && detail::Bounded(angular_velocity, -200, 200));
    const auto native = impl_->Body(body);
    b2Body_SetLinearVelocity(native, detail::Native(velocity));
    b2Body_SetAngularVelocity(native, angular_velocity);
    b2Body_SetAwake(native, true);
}
void World::SetPose(BodyHandle body, Vector position, float angle) {
    detail::Require(detail::Bounded(position) && detail::Bounded(angle, -10000, 10000));
    const auto native = impl_->Body(body);
    b2Body_SetTransform(native, detail::Native(position), b2MakeRot(angle));
    b2Body_SetAwake(native, true);
}
void World::ApplyImpulse(BodyHandle body, Vector impulse) {
    detail::Require(detail::Bounded(impulse, 10000));
    b2Body_ApplyLinearImpulseToCenter(impl_->Body(body), detail::Native(impulse), true);
}
void World::SetGravity(Vector gravity) {
    detail::Require(detail::Bounded(gravity, 100));
    if (gravity == impl_->config_.gravity_) return;
    b2World_SetGravity(impl_->native_.Get(), detail::Native(gravity));
    impl_->config_.gravity_ = gravity;
    for (const auto& [id, body] : impl_->bodies_) {
        (void)id;
        if (b2Body_GetType(body.native_.Get()) == b2_dynamicBody)
            b2Body_SetAwake(body.native_.Get(), true);
    }
}
std::size_t World::BodyCount() const { return impl_->bodies_.size(); }
std::size_t World::JointCount() const { return impl_->joints_.size(); }
StepResult World::Advance(double seconds) {
    detail::Require(std::isfinite(seconds) && seconds >= 0 && seconds <= 60, "physics.time");
    constexpr double step = 1.0 / 120;
    StepResult result;
    result.contacts_.reserve(256);
    result.catch_up_limited_ = seconds > 16 * step;
    impl_->accumulator_ += std::min(seconds, 16 * step);
    while (impl_->accumulator_ + 1e-12 >= step && result.steps_ < 16) {
        b2World_Step(impl_->native_.Get(), static_cast<float>(step), 4);
        impl_->ReadContacts(result);
        impl_->accumulator_ = std::max(0.0, impl_->accumulator_ - step);
        ++result.steps_;
    }
    return result;
}
}  // namespace rhythm::physics
