#include "point_physics.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <set>
#include <stdexcept>

namespace rhythm::runtime::detail {
namespace {
constexpr float kMetersPerHeight = 10;
float Gravity(const graph::Instruction& instruction, std::span<const NodeOutput> outputs,
              std::size_t port, std::string_view key, double fallback) {
    const double value = instruction.inputs_.at(port)
                                 ? outputs[*instruction.inputs_[port]].scalar_
                                 : graph::Scalar(instruction.node_, key, fallback);
    return static_cast<float>(std::isfinite(value) ? std::clamp(value, -10.0, 10.0) : fallback);
}
}  // namespace
void PointPhysics::Configure(const graph::Node& node, double aspect) {
    auto configuration = node;
    configuration.properties_.erase("gravity_x");
    configuration.properties_.erase("gravity_y");
    if (world_ && configuration_ == configuration && aspect_ == aspect) return;
    auto world = std::make_unique<physics::World>(physics::WorldConfig{{0, 0}, 516, 0});
    physics::BodyDefinition wall;
    wall.type_ = physics::BodyType::kStatic;
    wall.shape_ = physics::Shape::kBox;
    wall.friction_ = static_cast<float>(graph::Scalar(node, "friction", 0.3));
    wall.restitution_ = static_cast<float>(graph::Scalar(node, "restitution", 0.6));
    const auto width = static_cast<float>(aspect * kMetersPerHeight);
    const auto bounds = graph::Scalar(node, "physics_bounds", 2);
    if (bounds >= 1) {
        wall.position_ = {width * 0.5f, kMetersPerHeight + 0.1f};
        wall.half_extents_ = {width * 0.5f + 0.2f, 0.1f};
        world->CreateBody(wall);
    }
    if (bounds == 2) {
        wall.position_.y_ = -0.1f;
        world->CreateBody(wall);
        wall.half_extents_ = {0.1f, kMetersPerHeight * 0.5f + 0.2f};
        wall.position_ = {-0.1f, kMetersPerHeight * 0.5f};
        world->CreateBody(wall);
        wall.position_.x_ = width + 0.1f;
        world->CreateBody(wall);
    }
    world_ = std::move(world);
    bodies_.clear();
    configuration_ = std::move(configuration);
    aspect_ = aspect;
    last_seconds_.reset();
    ++generation_;
}
std::shared_ptr<const particles::PointCloud> PointPhysics::Evaluate(
        const graph::Instruction& instruction, std::span<const NodeOutput> outputs,
        FrameContext frame) {
    const auto& source = outputs[instruction.inputs_.at(0).value()].points_;
    if (!source || source->size() > 512) throw std::invalid_argument("runtime.physics_budget");
    const double aspect = static_cast<double>(frame.extent_.width_) / frame.extent_.height_;
    if (!std::isfinite(aspect) || aspect < 1.0 / 256 || aspect > 256)
        throw std::invalid_argument("runtime.physics_extent");
    // Validate the entire snapshot before changing body lifetime.
    std::set<std::uint64_t> live;
    for (const auto& point : *source) {
        if (!point.id_ || !live.insert(point.id_).second || !std::isfinite(point.x_) ||
            !std::isfinite(point.y_) || !std::isfinite(point.size_) || point.size_ < 0 ||
            !std::isfinite(point.rotation_) || !std::isfinite(point.velocity_x_) ||
            !std::isfinite(point.velocity_y_) || !std::isfinite(point.angular_velocity_))
            throw std::invalid_argument("runtime.physics_points");
    }
    const auto& input = outputs[instruction.inputs_.at(0).value()];
    if ((last_seconds_ && frame.seconds_ < *last_seconds_) || source_node_ != input.node_ ||
        source_generation_ != input.points_generation_)
        world_.reset();
    Configure(instruction.node_, aspect);
    source_node_ = input.node_;
    source_generation_ = input.points_generation_;
    world_->SetGravity({Gravity(instruction, outputs, 1, "gravity_x", 0) * kMetersPerHeight,
                        Gravity(instruction, outputs, 2, "gravity_y", 0.98) * kMetersPerHeight});
    std::erase_if(bodies_, [&](const auto& item) {
        if (live.contains(item.first)) return false;
        world_->DestroyBody(item.second.handle_);
        return true;
    });
    // Advance established bodies first; freshly published births already contain
    // their source's movement up to this frame and must not advance twice.
    if (frame.advance_state_ && last_seconds_)
        world_->Advance(std::clamp(frame.seconds_ - *last_seconds_, 0.0, 60.0));
    last_seconds_ = frame.seconds_;
    particles::PointCloud points = *source;
    const auto& node = instruction.node_;
    for (auto& point : points) {
        auto found = bodies_.find(point.id_);
        if (found != bodies_.end() && found->second.size_ != point.size_) {
            world_->DestroyBody(found->second.handle_);
            bodies_.erase(found);
            found = bodies_.end();
        }
        if (found == bodies_.end()) {
            physics::BodyDefinition definition;
            definition.position_ = {static_cast<float>(std::clamp(
                                            point.x_ * aspect * kMetersPerHeight, -9999.0, 9999.0)),
                                    std::clamp(point.y_ * kMetersPerHeight, -9999.0f, 9999.0f)};
            definition.velocity_ = {
                    static_cast<float>(std::clamp(point.velocity_x_ * aspect * kMetersPerHeight,
                                                  -200.0, 200.0)),
                    std::clamp(point.velocity_y_ * kMetersPerHeight, -200.0f, 200.0f)};
            definition.angle_ = std::remainder(point.rotation_, 2 * std::numbers::pi_v<float>);
            definition.angular_velocity_ = std::clamp(point.angular_velocity_, -200.0f, 200.0f);
            definition.radius_ = std::clamp(point.size_ * kMetersPerHeight * 0.5f, 0.005f, 80.0f);
            definition.half_extents_ = {definition.radius_, definition.radius_};
            definition.shape_ = graph::Scalar(node, "body_shape", 0) == 0 ? physics::Shape::kCircle
                                                                          : physics::Shape::kBox;
            definition.density_ = static_cast<float>(graph::Scalar(node, "density", 1));
            definition.friction_ = static_cast<float>(graph::Scalar(node, "friction", 0.3));
            definition.restitution_ = static_cast<float>(graph::Scalar(node, "restitution", 0.6));
            found = bodies_.emplace(point.id_, Body{world_->CreateBody(definition), point.size_})
                            .first;
        }
        const auto body = world_->ReadBody(found->second.handle_);
        point.x_ = static_cast<float>(body.position_.x_ / (aspect * kMetersPerHeight));
        point.y_ = body.position_.y_ / kMetersPerHeight;
        point.rotation_ = body.angle_;
        point.velocity_x_ = static_cast<float>(body.velocity_.x_ / (aspect * kMetersPerHeight));
        point.velocity_y_ = body.velocity_.y_ / kMetersPerHeight;
        point.angular_velocity_ = body.angular_velocity_;
    }
    return std::make_shared<const particles::PointCloud>(std::move(points));
}
}  // namespace rhythm::runtime::detail
