#include "rhythm/runtime/runtime.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <set>
#include <stdexcept>

#include "audio_spectrum.h"
#include "rhythm/render/layout.h"
#include "runtime_state.h"
#include "scalar_ops.h"
#include "scene_ops.h"
#include "texture_ops.h"

namespace rhythm::runtime {
void Runtime::Impl::Reset() {
    states_.clear();
    white_ = {};
    point_sprite_ = {};
    document_id_.clear();
    extent_ = {};
}

FrameResult Runtime::Impl::Evaluate(const graph::ExecutionPlan& plan, FrameContext frame,
                                    render::Renderer& renderer) {
    if (!std::isfinite(frame.seconds_) || frame.seconds_ < 0 || !frame.extent_.width_ ||
        !frame.extent_.height_ || plan.instructions_.empty() ||
        plan.output_ >= plan.instructions_.size())
        throw std::invalid_argument("runtime.frame");
    if (!ValidExternalInputs(frame.external_))
        throw std::invalid_argument("runtime.external_inputs");
    if (graph::ValidatePointBudget(plan)) throw std::length_error("runtime.points_budget");
    static const scene::Resources kNoResources;
    const auto& resources = frame.resources_ ? *frame.resources_ : kNoResources;
    const auto geometry_budgets = detail::GeometryBudgets(plan, resources);
    if (graph::ValidateSceneBudget(plan, geometry_budgets))
        throw std::length_error("runtime.scene_budget");
    if (document_id_ != plan.document_id_ || reset_generation_ != frame.reset_generation_ ||
        extent_ != frame.extent_) {
        Reset();
        document_id_ = plan.document_id_;
        reset_generation_ = frame.reset_generation_;
        extent_ = frame.extent_;
    }
    if (!renderer.IsValid(white_.Handle())) {
        if (white_.Handle() != render::TextureHandle{})
            throw std::invalid_argument("runtime.device_changed");
        const std::array<std::uint8_t, 4> white{255, 255, 255, 255};
        white_ = renderer.CreateTexture({1, 1}, white);
    }
    std::set<graph::NodeId> active;
    for (const auto& instruction : plan.instructions_) active.insert(instruction.node_.id_);
    std::erase_if(states_, [&](const auto& item) { return !active.contains(item.first); });
    FrameResult result;
    const auto presentation_generation = renderer.Stats().presentation_generation_;
    const bool redraw = presentation_generation != presentation_generation_;
    presentation_generation_ = presentation_generation;
    result.extent_ = frame.extent_;
    result.outputs_.resize(plan.instructions_.size());
    // Include feedback writes in the output closure: pausing a viewer must not
    // pause history which contributes to the final image.
    std::vector<bool> primary(plan.instructions_.size(), false);
    std::vector<std::size_t> pending{plan.output_};
    while (!pending.empty()) {
        const auto index = pending.back();
        pending.pop_back();
        if (primary.at(index)) continue;
        primary[index] = true;
        for (const auto input : plan.instructions_[index].inputs_)
            if (input) pending.push_back(*input);
    }
    for (std::size_t index = 0; index < plan.instructions_.size(); ++index) {
        const auto& instruction = plan.instructions_[index];
        const auto& node = instruction.node_;
        auto& state = states_[node.id_];
        if (!primary[index] && !frame.evaluate_viewers_) {
            result.outputs_[index] = state.output_;
            continue;
        }
        auto extent = frame.extent_;
        if (!primary[index]) {
            const auto fitted =
                    render::AspectFit(frame.extent_, {0, 0, float(frame.viewer_extent_.width_),
                                                      float(frame.viewer_extent_.height_)});
            extent = {static_cast<std::uint16_t>(std::max(1.0f, std::round(fitted.width_))),
                      static_cast<std::uint16_t>(std::max(1.0f, std::round(fitted.height_)))};
        }
        const auto operation = instruction.operation_;
        if ((state.node_ && state.node_->type_ != node.type_) || state.extent_ != extent) {
            state = {};
            state.extent_ = extent;
        }
        const auto input = [&](std::size_t port) -> const NodeOutput& {
            return result.outputs_.at(instruction.inputs_.at(port).value());
        };
        std::vector<std::uint64_t> versions;
        if (operation != graph::Operation::kFeedback)
            for (const auto source : instruction.inputs_) {
                const auto value = source ? result.outputs_.at(*source) : NodeOutput{};
                versions.insert(versions.end(),
                                {value.node_, value.version_, value.texture_.device_,
                                 value.texture_.slot_, value.texture_.generation_});
            }
        if (operation == graph::Operation::kTime ||
            operation == graph::Operation::kParticleEmitter ||
            operation == graph::Operation::kPointPhysics)
            versions.push_back(std::bit_cast<std::uint64_t>(frame.seconds_));
        if (operation == graph::Operation::kParticleEmitter ||
            operation == graph::Operation::kPointPhysics)
            versions.push_back(frame.advance_state_ ? 1 : 0);
        const auto external_value = detail::ExternalScalar(instruction, frame);
        if (external_value) versions.push_back(std::bit_cast<std::uint64_t>(*external_value));
        if (operation == graph::Operation::kAudioSpectrum)
            for (const auto band : detail::SpectrumBands(node, frame.external_))
                versions.push_back(std::bit_cast<std::uint32_t>(band));
        const bool dirty = redraw || !state.node_ || *state.node_ != node ||
                           state.input_versions_ != versions ||
                           (operation == graph::Operation::kFeedback && frame.advance_state_);
        if (dirty) {
            ++result.evaluated_;
            state.output_.node_ = node.id_;
            state.output_.version_ = next_output_version_++;
            render::DrawList list;
            list.width_ = extent.width_;
            list.height_ = extent.height_;
            switch (operation) {
                case graph::Operation::kGaussianBlur: {
                    if (!state.blur_) state.blur_ = std::make_unique<detail::BlurPass>();
                    const auto value = instruction.inputs_[1]
                                               ? input(1).scalar_
                                               : graph::Scalar(node, "blur_radius", 6);
                    // Authoring radius uses pixels at a 720-pixel short edge so
                    // inline previews and portrait/square profiles keep the same look.
                    const auto radius = static_cast<float>(std::clamp(value, 0.0, 32.0)) *
                                        std::min(extent.width_, extent.height_) / 720.0f;
                    state.output_.texture_ =
                            state.blur_->Draw(input(0).texture_, extent, radius, renderer);
                    break;
                }
                case graph::Operation::kGeometryCube:
                case graph::Operation::kGeometrySphere:
                case graph::Operation::kGeometryGlb:
                case graph::Operation::kMaterialUnlit:
                case graph::Operation::kMaterialPbr:
                case graph::Operation::kDirectionalLight:
                case graph::Operation::kSceneInstance:
                case graph::Operation::kSceneTransform:
                case graph::Operation::kSceneMerge:
                case graph::Operation::kSceneCamera:
                    detail::EvaluateScene(instruction, result.outputs_, state.output_, resources);
                    break;
                case graph::Operation::kSceneRender: {
                    if (!input(0).scene_) throw std::invalid_argument("runtime.scene_input");
                    if (!state.scene_) state.scene_ = std::make_unique<detail::ScenePass>();
                    if (!state.target_.Handle().device_)
                        state.target_ = renderer.CreateTexture(extent);
                    const auto camera =
                            instruction.inputs_[1] ? input(1).camera_.value() : scene::Camera{};
                    const auto scene_draw =
                            state.scene_->Build(*input(0).scene_, camera, extent, renderer);
                    renderer.SubmitScene(state.target_.Handle(), scene_draw);
                    state.output_.texture_ = state.target_.Handle();
                    break;
                }
                case graph::Operation::kPointGrid:
                    state.output_.points_ = detail::GridPoints(node);
                    state.output_.points_generation_ = next_points_generation_++;
                    break;
                case graph::Operation::kParticleEmitter:
                    if (!state.points_) state.points_ = std::make_unique<detail::PointState>();
                    if (!state.output_.points_generation_ || !state.node_ ||
                        graph::Scalar(*state.node_, "seed", 1) != graph::Scalar(node, "seed", 1) ||
                        (state.points_->last_seconds_ &&
                         frame.seconds_ < *state.points_->last_seconds_))
                        state.output_.points_generation_ = next_points_generation_++;
                    state.output_.points_ =
                            detail::EmitPoints(*state.points_, instruction, result.outputs_, frame);
                    break;
                case graph::Operation::kPointTransform:
                    state.output_.points_generation_ = input(0).points_generation_;
                    state.output_.points_ = detail::TransformPoints(
                            instruction, result.outputs_,
                            static_cast<double>(extent.width_) / extent.height_);
                    break;
                case graph::Operation::kPointPhysics: {
                    if (!state.physics_) state.physics_ = std::make_unique<detail::PointPhysics>();
                    const auto generation = state.physics_->Generation();
                    state.output_.points_ =
                            state.physics_->Evaluate(instruction, result.outputs_, frame);
                    if (generation != state.physics_->Generation())
                        state.output_.points_generation_ = next_points_generation_++;
                    break;
                }
                case graph::Operation::kPointRender: {
                    if (!input(0).points_) throw std::invalid_argument("runtime.points");
                    if (!state.target_.Handle().device_)
                        state.target_ = renderer.CreateTexture(extent);
                    auto sprite = white_.Handle();
                    if (instruction.inputs_[1])
                        sprite = input(1).texture_;
                    else if (graph::Scalar(node, "point_style", 0) == 0) {
                        if (!point_sprite_.Handle().device_)
                            point_sprite_ = detail::CreatePointSprite(renderer);
                        sprite = point_sprite_.Handle();
                    }
                    detail::DrawPoints(*input(0).points_, sprite,
                                       graph::Scalar(node, "point_blend", 0) == 0
                                               ? render::BlendMode::kSourceOver
                                               : render::BlendMode::kAdd,
                                       list);
                    renderer.Submit(state.target_.Handle(), list);
                    state.output_.texture_ = state.target_.Handle();
                    break;
                }
                case graph::Operation::kSessionTime:
                case graph::Operation::kParticipantRole:
                case graph::Operation::kSharedControl:
                case graph::Operation::kAudioFeature:
                case graph::Operation::kAudioBand:
                    state.output_.scalar_ = external_value.value();
                    break;
                case graph::Operation::kTime:
                case graph::Operation::kOscillator:
                case graph::Operation::kSample:
                case graph::Operation::kConstant:
                case graph::Operation::kExpression:
                case graph::Operation::kMath:
                case graph::Operation::kLocalTime:
                case graph::Operation::kCurve:
                case graph::Operation::kMap:
                case graph::Operation::kCompare:
                case graph::Operation::kSelect:
                case graph::Operation::kNoise:
                    state.output_.scalar_ =
                            detail::EvaluateScalar(instruction, result.outputs_, frame.seconds_);
                    break;
                case graph::Operation::kOutput:
                    state.output_.texture_ = input(0).texture_;
                    break;
                case graph::Operation::kFeedback:
                    if (!state.target_.Handle().device_) {
                        state.target_ = renderer.CreateTexture(extent);
                        state.history_ = renderer.CreateTexture(extent);
                        renderer.Submit(state.target_.Handle(), list, 0x000000ff);
                        renderer.Submit(state.history_.Handle(), list, 0x000000ff);
                    }
                    state.output_.texture_ = state.history_.Handle();
                    break;
                default:
                    if (!state.target_.Handle().device_)
                        state.target_ = renderer.CreateTexture(extent);
                    const auto clear = detail::DrawTexture(instruction, result.outputs_,
                                                           frame.external_, white_.Handle(), list);
                    renderer.Submit(state.target_.Handle(), list, clear);
                    state.output_.texture_ = state.target_.Handle();
                    break;
            }
            state.node_ = node;
            state.input_versions_ = std::move(versions);
        }
        result.outputs_[index] = state.output_;
    }
    // Feedback reads have finished. Write into the other texture, then swap for next frame.
    for (std::size_t index = 0; index < plan.instructions_.size(); ++index) {
        const auto& instruction = plan.instructions_[index];
        if (!primary[index] && !frame.evaluate_viewers_) continue;
        if (instruction.operation_ != graph::Operation::kFeedback) continue;
        if (!frame.advance_state_) continue;
        auto& state = states_.at(instruction.node_.id_);
        const auto source = result.outputs_.at(instruction.inputs_.at(0).value()).texture_;
        render::DrawList list;
        list.width_ = state.extent_.width_;
        list.height_ = state.extent_.height_;
        detail::AppendTextureQuad(list, source, 0xffffffff, 0xffffffff);
        renderer.Submit(state.target_.Handle(), list, 0x000000ff);
        std::swap(state.target_, state.history_);
    }
    result.final_ = result.outputs_.at(plan.output_).texture_;
    return result;
}
}  // namespace rhythm::runtime
