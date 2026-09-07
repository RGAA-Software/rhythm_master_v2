#include "world_impl.h"

namespace rhythm::physics {
JointHandle World::CreateDistance(const DistanceJoint& definition) {
    using detail::Bounded;
    detail::Require(impl_->joints_.size() < impl_->config_.joint_limit_, "physics.joint_limit");
    detail::Require(
            definition.first_ != definition.second_ && Bounded(definition.first_anchor_, 100) &&
            Bounded(definition.second_anchor_, 100) && Bounded(definition.length_, 0.01f, 100) &&
            Bounded(definition.spring_hertz_, 0, 60) && Bounded(definition.damping_, 0, 10));
    detail::Require(impl_->next_joint_ != UINT64_MAX, "physics.id_exhausted");
    auto native = b2DefaultDistanceJointDef();
    native.bodyIdA = impl_->Body(definition.first_);
    native.bodyIdB = impl_->Body(definition.second_);
    native.localAnchorA = detail::Native(definition.first_anchor_);
    native.localAnchorB = detail::Native(definition.second_anchor_);
    native.length = definition.length_;
    native.enableSpring = definition.spring_hertz_ > 0;
    native.hertz = definition.spring_hertz_;
    native.dampingRatio = definition.damping_;
    native.collideConnected = definition.collide_;
    Impl::JointRecord record{
            detail::NativeJoint(b2CreateDistanceJoint(impl_->native_.Get(), &native)),
            definition.first_, definition.second_};
    detail::Require(b2Joint_IsValid(record.native_.Get()), "physics.create_joint");
    const JointHandle handle{impl_->identity_, impl_->next_joint_++};
    impl_->joints_.emplace(handle.id_, std::move(record));
    return handle;
}
JointHandle World::CreateHinge(const HingeJoint& definition) {
    using detail::Bounded;
    detail::Require(impl_->joints_.size() < impl_->config_.joint_limit_, "physics.joint_limit");
    detail::Require(definition.first_ != definition.second_ &&
                    Bounded(definition.first_anchor_, 100) &&
                    Bounded(definition.second_anchor_, 100) &&
                    Bounded(definition.reference_angle_, -100, 100) &&
                    Bounded(definition.motor_speed_, -100, 100) &&
                    Bounded(definition.motor_torque_, 0, 100000));
    detail::Require(impl_->next_joint_ != UINT64_MAX, "physics.id_exhausted");
    auto native = b2DefaultRevoluteJointDef();
    native.bodyIdA = impl_->Body(definition.first_);
    native.bodyIdB = impl_->Body(definition.second_);
    native.localAnchorA = detail::Native(definition.first_anchor_);
    native.localAnchorB = detail::Native(definition.second_anchor_);
    native.referenceAngle = definition.reference_angle_;
    native.enableMotor = definition.motor_torque_ > 0;
    native.motorSpeed = definition.motor_speed_;
    native.maxMotorTorque = definition.motor_torque_;
    native.collideConnected = definition.collide_;
    Impl::JointRecord record{
            detail::NativeJoint(b2CreateRevoluteJoint(impl_->native_.Get(), &native)),
            definition.first_, definition.second_};
    detail::Require(b2Joint_IsValid(record.native_.Get()), "physics.create_joint");
    const JointHandle handle{impl_->identity_, impl_->next_joint_++};
    impl_->joints_.emplace(handle.id_, std::move(record));
    return handle;
}
bool World::Contains(JointHandle joint) const {
    return joint.world_ == impl_->identity_ && impl_->joints_.contains(joint.id_);
}
void World::DestroyJoint(JointHandle joint) {
    detail::Require(Contains(joint), "physics.joint_handle");
    impl_->joints_.erase(joint.id_);
}
}  // namespace rhythm::physics
