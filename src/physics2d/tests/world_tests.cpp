#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "rhythm/physics/world.h"

#ifdef RHYTHM_NATIVE_PHYSICS_PROBE
void CheckNativeDistance();
#endif

namespace {
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
template <typename Function>
void Reject(Function function) {
    bool rejected = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    Require(rejected, "invalid physics operation must reject");
}
void Run() {
    using namespace rhythm::physics;
    World world;
    BodyDefinition floor;
    floor.type_ = BodyType::kStatic;
    floor.shape_ = Shape::kBox;
    floor.position_ = {0, 2};
    floor.half_extents_ = {4, 0.1f};
    const auto ground = world.CreateBody(floor);
    BodyDefinition ball;
    ball.radius_ = 0.2f;
    ball.restitution_ = 0.3f;
    const auto body = world.CreateBody(ball);
    std::size_t hits = 0, begins = 0;
    for (int frame = 0; frame < 180; ++frame) {
        const auto update = world.Advance(1.0 / 60);
        Require(update.steps_ == 2 && !update.catch_up_limited_, "fixed-step cadence");
        for (const auto& contact : update.contacts_) {
            Require(world.Contains(contact.first_) && world.Contains(contact.second_),
                    "contact stable handles");
            hits += contact.kind_ == ContactKind::kHit ? 1 : 0;
            begins += contact.kind_ == ContactKind::kBegin ? 1 : 0;
        }
    }
    const auto rested = world.ReadBody(body);
    Require(rested.position_.y_ > 1.6f && rested.position_.y_ < 1.8f && hits && begins,
            "falling circle collides with ground and emits contact/hit events");
    World foreign;
    Require(!foreign.Contains(body), "cross-world handles reject");
    Reject([&] { foreign.ReadBody(body); });
    world.SetGravity({0, -9.8f});
    world.Advance(0.1);
    Require(world.ReadBody(body).position_.y_ < rested.position_.y_,
            "gravity changes wake sleeping bodies");
    const auto count = world.BodyCount();
    auto invalid = ball;
    invalid.radius_ = -1;
    Reject([&] { world.CreateBody(invalid); });
    Require(world.BodyCount() == count, "invalid creation preserves world");
    Reject([&] { world.Advance(std::numeric_limits<double>::quiet_NaN()); });
    Require(world.Advance(10).steps_ == 16, "catch-up work bounded");
    world.DestroyBody(body);
    Require(!world.Contains(body), "destroyed handles invalidated");
    const auto replacement = world.CreateBody(ball);
    Require(replacement.id_ > body.id_ && world.Contains(ground), "body identities never reused");
    Reject([&] { world.ApplyImpulse(body, {1, 0}); });
    World constraints({{0, 0}, 16, 16});
    BodyDefinition fixed;
    fixed.type_ = BodyType::kStatic;
    const auto anchor = constraints.CreateBody(fixed);
    ball.position_ = {2, 0};
    const auto bob = constraints.CreateBody(ball);
    DistanceJoint distance;
    distance.first_ = anchor;
    distance.second_ = bob;
    distance.length_ = 1;
    const auto joint = constraints.CreateDistance(distance);
    for (int frame = 0; frame < 120; ++frame) constraints.Advance(1.0 / 120);
    const auto pose = constraints.ReadBody(bob).position_;
    Require(std::abs(std::hypot(pose.x_, pose.y_) - 1) < 0.01f, "distance constraint");
    constraints.DestroyJoint(joint);
    Require(!constraints.Contains(joint), "joint handle invalidated");
    constraints.SetPose(bob, {0, 0}, 0);
    HingeJoint hinge;
    hinge.first_ = anchor;
    hinge.second_ = bob;
    hinge.motor_speed_ = 2;
    hinge.motor_torque_ = 10;
    const auto motor = constraints.CreateHinge(hinge);
    for (int frame = 0; frame < 120; ++frame) constraints.Advance(1.0 / 120);
    Require(std::abs(constraints.ReadBody(bob).angle_) > 1, "hinge motor");
    constraints.DestroyBody(bob);
    Require(!constraints.Contains(motor) && constraints.JointCount() == 0,
            "body removal owns connected joint cleanup");
    World sensors({{0, 0}, 16, 16});
    floor.position_ = {0, 0};
    floor.half_extents_ = {0.5f, 0.5f};
    floor.sensor_ = true;
    sensors.CreateBody(floor);
    ball.position_ = {-2, 0};
    ball.velocity_ = {2, 0};
    const auto visitor = sensors.CreateBody(ball);
    std::size_t enters = 0, leaves = 0;
    for (int frame = 0; frame < 240; ++frame)
        for (const auto& contact : sensors.Advance(1.0 / 120).contacts_) {
            enters += contact.kind_ == ContactKind::kSensorBegin ? 1 : 0;
            leaves += contact.kind_ == ContactKind::kSensorEnd ? 1 : 0;
        }
    Require(enters == 1 && leaves == 1 && sensors.ReadBody(visitor).position_.x_ > 1.9f,
            "sensor enter/leave without collision response");
    World moved(std::move(sensors));
    Require(moved.Contains(visitor), "world move preserves stable identities");
    std::cout << "physics2d: collision/hit/sensor values, handles, gravity, limits, distance/hinge "
                 "joints and RAII passed\n";
}
}  // namespace
int main() {
    try {
#ifdef RHYTHM_NATIVE_PHYSICS_PROBE
        CheckNativeDistance();
#endif
        Run();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
