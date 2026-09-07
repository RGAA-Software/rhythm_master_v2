#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace rhythm::physics {
struct Vector {
    float x_ = 0;
    float y_ = 0;
    bool operator==(const Vector&) const = default;
};
struct BodyHandle {
    std::uint64_t world_ = 0;
    std::uint64_t id_ = 0;
    bool operator==(const BodyHandle&) const = default;
};
struct JointHandle {
    std::uint64_t world_ = 0;
    std::uint64_t id_ = 0;
    bool operator==(const JointHandle&) const = default;
};
enum class BodyType { kStatic, kKinematic, kDynamic };
enum class Shape { kCircle, kBox };
struct BodyDefinition {
    BodyType type_ = BodyType::kDynamic;
    Shape shape_ = Shape::kCircle;
    Vector position_{};
    Vector velocity_{};
    float angle_ = 0;
    float angular_velocity_ = 0;
    float radius_ = 0.1f;
    Vector half_extents_{0.1f, 0.1f};
    float density_ = 1;
    float friction_ = 0.3f;
    float restitution_ = 0.5f;
    float gravity_scale_ = 1;
    bool sensor_ = false;
    bool bullet_ = false;
    std::uint64_t category_ = 1;
    std::uint64_t mask_ = UINT64_MAX;
};
struct BodyState {
    Vector position_{};
    Vector velocity_{};
    float angle_ = 0;
    float angular_velocity_ = 0;
    bool awake_ = false;
};
struct WorldConfig {
    Vector gravity_{0, 9.8f};
    std::uint32_t body_limit_ = 1024;
    std::uint32_t joint_limit_ = 1024;
    bool sleep_ = true;
};
struct DistanceJoint {
    BodyHandle first_{};
    BodyHandle second_{};
    Vector first_anchor_{};
    Vector second_anchor_{};
    float length_ = 1;
    float spring_hertz_ = 0;
    float damping_ = 0.7f;
    bool collide_ = false;
};
struct HingeJoint {
    BodyHandle first_{};
    BodyHandle second_{};
    Vector first_anchor_{};
    Vector second_anchor_{};
    float reference_angle_ = 0;
    float motor_speed_ = 0;
    float motor_torque_ = 0;
    bool collide_ = false;
};
enum class ContactKind { kBegin, kEnd, kHit, kSensorBegin, kSensorEnd };
struct Contact {
    ContactKind kind_ = ContactKind::kBegin;
    BodyHandle first_{};
    BodyHandle second_{};
    Vector position_{};
    Vector normal_{};
    float approach_speed_ = 0;
};
struct StepResult {
    std::uint32_t steps_ = 0;
    bool catch_up_limited_ = false;
    bool events_limited_ = false;
    std::vector<Contact> contacts_{};
};
// Single host-thread owner. Positions are meters and angles are radians; the
// caller chooses the up axis through gravity. Handles are non-owning values,
// never reused within a world and rejected across worlds or after destruction.
class World final {
   public:
    explicit World(WorldConfig config = {});
    ~World();
    World(World&&) noexcept;
    World& operator=(World&&) noexcept;
    World(const World&) = delete;
    World& operator=(const World&) = delete;
    BodyHandle CreateBody(const BodyDefinition& definition);
    bool Contains(BodyHandle body) const;
    void DestroyBody(BodyHandle body);
    BodyState ReadBody(BodyHandle body) const;
    void SetVelocity(BodyHandle body, Vector velocity, float angular_velocity = 0);
    void SetPose(BodyHandle body, Vector position, float angle);
    void ApplyImpulse(BodyHandle body, Vector impulse);
    JointHandle CreateDistance(const DistanceJoint& definition);
    JointHandle CreateHinge(const HingeJoint& definition);
    bool Contains(JointHandle joint) const;
    void DestroyJoint(JointHandle joint);
    void SetGravity(Vector gravity);
    // 120 Hz, four solver substeps, at most 16 steps and 4096 contact records.
    // Events concerning already destroyed bodies are omitted.
    StepResult Advance(double seconds);
    std::size_t BodyCount() const;
    std::size_t JointCount() const;

   private:
    class Impl;
    std::unique_ptr<Impl> impl_{};
};
}  // namespace rhythm::physics
