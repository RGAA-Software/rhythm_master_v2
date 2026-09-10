#include "rhythm/runtime/runtime.h"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <set>
#include <stdexcept>

#include "audio_spectrum.h"
#include "rhythm/render/layout.h"
#include "runtime_state.h"
#include "scalar_ops.h"
#include "scene_ops.h"
#include "spectrum_points.h"
#include "texture_ops.h"

namespace rhythm::runtime {
void Runtime::Impl::ResetResources() {
    failure_.reset();
    lifetimes_ = {};
    targets_ = {};
    paused_frame_.reset();
    states_.clear();
    white_ = {};
    point_sprite_ = {};
    images_ = {};
    shaders_ = {};
    surfaces_ = {};
    videos_ = {};
    vectors_ = {};
    document_id_.clear();
    extent_ = {};
}

FrameResult Runtime::Impl::EvaluateRange(
        const graph::ExecutionPlan& plan, FrameContext frame, render::Renderer& renderer,
        const std::optional<std::reference_wrapper<Preparation>>& preparation,
        PreparationBudget budget) {
    const auto step_started = std::chrono::steady_clock::now();
    if (!std::isfinite(frame.seconds_) || frame.seconds_ < 0 || !frame.extent_.width_ ||
        !frame.extent_.height_ || plan.instructions_.empty() ||
        plan.output_ >= plan.instructions_.size())
        throw std::invalid_argument("runtime.frame");
    if (!ValidExternalInputs(frame.external_))
        throw std::invalid_argument("runtime.external_inputs");
    const auto motion = frame.motion_.value_or(MotionTime{frame.seconds_, frame.reset_generation_});
    if (!std::isfinite(motion.seconds_) || motion.seconds_ < 0)
        throw std::invalid_argument("runtime.motion_time");
    frame.external_.controls_ = parameters::EvaluateControls(
            plan.controls_, plan.control_sequence_, frame.seconds_, frame.external_.controls_);
    auto simulation_frame = frame;
    simulation_frame.seconds_ = motion.seconds_;
    if (graph::ValidatePointBudget(plan)) throw std::length_error("runtime.points_budget");
    static const scene::Resources kNoResources;
    const auto& resources = frame.resources_ ? *frame.resources_ : kNoResources;
    static const assets::Images kNoImages;
    const auto& images = frame.images_ ? *frame.images_ : kNoImages;
    const auto geometry_budgets = detail::GeometryBudgets(plan, resources);
    if (graph::ValidateSceneBudget(plan, geometry_budgets))
        throw std::length_error("runtime.scene_budget");
    static const image_shader::Resources kNoShaders;
    const auto& shaders = frame.shaders_ ? *frame.shaders_ : kNoShaders;
    static const surface_shader::Resources kNoSurfaces;
    const auto& surfaces = frame.surfaces_ ? *frame.surfaces_ : kNoSurfaces;
    FrameResult result;
    const auto presentation_generation = renderer.Stats().presentation_generation_;
    const bool initialized = preparation && preparation->get().initialized_;
    const bool redraw = initialized ? preparation->get().redraw_
                                    : presentation_generation != presentation_generation_;
    if (initialized) {
        if (!renderer.IsValid(white_.Handle()) ||
            presentation_generation != preparation->get().presentation_generation_)
            throw std::invalid_argument("runtime.preparation_device_changed");
        result = std::move(preparation->get().partial_);
    } else {
        if (document_id_ != plan.document_id_) phases_.clear();
        const bool continuous_boundary = frame.preserve_history_ && frame.motion_ &&
                                         motion_generation_ == motion.generation_ &&
                                         frame.reset_generation_ == reset_generation_ + 1;
        if (document_id_ != plan.document_id_ ||
            (reset_generation_ != frame.reset_generation_ && !continuous_boundary) ||
            extent_ != frame.extent_) {
            ResetResources();
            document_id_ = plan.document_id_;
            reset_generation_ = frame.reset_generation_;
            extent_ = frame.extent_;
        }
        reset_generation_ = frame.reset_generation_;
        motion_generation_ = frame.motion_ ? std::optional(motion.generation_) : std::nullopt;
        if (!renderer.IsValid(white_.Handle())) {
            if (white_.Handle() != render::TextureHandle{})
                throw std::invalid_argument("runtime.device_changed");
            const std::array<std::uint8_t, 4> white{255, 255, 255, 255};
            white_ = renderer.CreateTexture({1, 1}, white);
        }
        images_.Retain(plan, images);
        vectors_.Retain(plan);
        shaders_.Retain(plan, shaders);
        surfaces_.Retain(plan, surfaces);
        if (preparation)
            videos_.Retain(plan);
        else
            videos_.Prepare(plan, frame.videos_, renderer);
        std::set<graph::NodeId> active;
        std::set<graph::NodeId> active_phases;
        for (const auto& instruction : plan.instructions_) {
            active.insert(instruction.node_.id_);
            if (instruction.operation_ == graph::Operation::kMotionPhase)
                active_phases.insert(instruction.node_.id_);
        }
        std::erase_if(states_, [&](const auto& item) { return !active.contains(item.first); });
        std::erase_if(phases_,
                      [&](const auto& item) { return !active_phases.contains(item.first); });
        presentation_generation_ = presentation_generation;
        result.extent_ = frame.extent_;
        result.outputs_.resize(plan.instructions_.size());
        if (frame.profile_nodes_) result.profiles_.resize(plan.instructions_.size());
        lifetimes_.Prepare(plan, frame.retained_textures_);
        if (!frame.advance_state_ && paused_frame_ && paused_frame_->frame_ == frame &&
            paused_frame_->plan_generation_ == lifetimes_.Generation() &&
            paused_frame_->presentation_generation_ == presentation_generation) {
            auto cached = paused_frame_->result_;
            cached.evaluated_ = 0;
            cached.recycled_textures_ = 0;
            for (auto& profile : cached.profiles_) profile = {profile.node_};
            return cached;
        }
        paused_frame_.reset();
        targets_.BeginFrame();
        if (preparation) {
            preparation->get().initialized_ = true;
            preparation->get().redraw_ = redraw;
            preparation->get().presentation_generation_ = presentation_generation;
            preparation->get().frame_ = frame;
        }
    }
    std::size_t processed = 0;
    const auto checkpoint = [&](std::size_t next,
                                std::chrono::steady_clock::time_point node_started) {
        if (!preparation) return false;
        auto& cursor = preparation->get();
        const auto now = std::chrono::steady_clock::now();
        cursor.next_ = next;
        cursor.maximum_node_ms_ =
                std::max(cursor.maximum_node_ms_,
                         std::chrono::duration<double, std::milli>(now - node_started).count());
        ++processed;
        if (next < plan.instructions_.size() &&
            (processed >= budget.maximum_nodes_ ||
             std::chrono::duration<double, std::milli>(now - step_started).count() >=
                     budget.maximum_cpu_ms_)) {
            cursor.partial_ = std::move(result);
            return true;
        }
        return false;
    };
    const auto retire = [&](std::size_t index) {
        for (const auto retired : lifetimes_.RetireAfter(index)) {
            auto& state = states_.at(plan.instructions_[retired].node_.id_);
            if (!state.target_.Handle().device_) continue;
            targets_.Recycle(std::move(state.target_), state.extent_, state.precision_);
            state.target_retired_ = true;
            state.output_.texture_ = {};
            result.outputs_[retired].texture_ = {};
            ++result.recycled_textures_;
        }
    };
    const auto acquire = [&](render::Extent extent, render::TexturePrecision precision) {
        return lifetimes_.Enabled() ? targets_.Acquire(extent, renderer, precision)
                                    : renderer.CreateTexture(extent, {}, precision);
    };
    const auto begin = preparation ? preparation->get().next_ : 0;
    for (std::size_t index = begin; index < plan.instructions_.size(); ++index) {
        const auto& instruction = plan.instructions_[index];
        const auto& node = instruction.node_;
        using Clock = std::chrono::steady_clock;
        const auto profile_start =
                frame.profile_nodes_ || preparation ? Clock::now() : Clock::time_point{};
        const auto before = frame.profile_nodes_ ? renderer.Stats() : render::FrameStats{};
        if (frame.profile_nodes_) result.profiles_[index].node_ = node.id_;
        auto& state = states_[node.id_];
        if (!lifetimes_.Primary(index) && !frame.evaluate_viewers_) {
            result.outputs_[index] = state.output_;
            retire(index);
            if (checkpoint(index + 1, profile_start)) return {};
            continue;
        }
        auto extent = frame.extent_;
        if (!lifetimes_.Primary(index)) {
            const auto fitted =
                    render::AspectFit(frame.extent_, {0, 0, float(frame.viewer_extent_.width_),
                                                      float(frame.viewer_extent_.height_)});
            extent = {static_cast<std::uint16_t>(std::max(1.0f, std::round(fitted.width_))),
                      static_cast<std::uint16_t>(std::max(1.0f, std::round(fitted.height_)))};
        }
        const auto operation = instruction.operation_;
        if (preparation && operation == graph::Operation::kTextureVideo)
            videos_.PrepareNode(instruction, frame.videos_, renderer);
        const auto precision = detail::OutputPrecision(instruction, result.outputs_, renderer);
        if ((state.node_ && state.node_->type_ != node.type_) || state.extent_ != extent ||
            state.precision_ != precision) {
            state = {};
            state.extent_ = extent;
            state.precision_ = precision;
        }
        const auto input = [&](std::size_t port) -> const NodeOutput& {
            return result.outputs_.at(instruction.inputs_.at(port).value());
        };
        std::vector<std::uint64_t> versions;
        if (operation == graph::Operation::kMotionPhase) {
            versions.insert(versions.end(),
                            {std::bit_cast<std::uint64_t>(motion.seconds_), motion.generation_,
                             frame.advance_state_ ? 1ull : 0ull});
        }
        const NodeOutput empty_input;
        for (std::size_t port = 0; port < instruction.inputs_.size(); ++port) {
            if (operation == graph::Operation::kFeedback && port == 0) continue;
            const auto source = instruction.inputs_[port];
            const auto& value = source ? result.outputs_.at(*source) : empty_input;
            versions.insert(versions.end(), {value.node_, value.version_, value.texture_.device_,
                                             value.texture_.slot_, value.texture_.generation_});
        }
        if (operation == graph::Operation::kTime ||
            ((operation == graph::Operation::kTextureShader ||
              operation == graph::Operation::kMaterialShader) &&
             !instruction.inputs_[1]) ||
            (operation == graph::Operation::kGeometryAnimate && !instruction.inputs_[1]))
            versions.push_back(std::bit_cast<std::uint64_t>(frame.seconds_));
        if (operation == graph::Operation::kParticleEmitter ||
            operation == graph::Operation::kGpuParticleEmitter ||
            operation == graph::Operation::kTextureTrail ||
            operation == graph::Operation::kPointPhysics) {
            versions.push_back(std::bit_cast<std::uint64_t>(motion.seconds_));
            versions.push_back(frame.advance_state_ ? 1 : 0);
        }
        if (operation == graph::Operation::kTextureVideo)
            versions.push_back(videos_.Revision(node.id_));
        if (detail::IsEventOperation(operation)) {
            versions.push_back(frame.reset_generation_);
            versions.push_back(std::bit_cast<std::uint64_t>(frame.seconds_));
            versions.push_back(frame.advance_state_ ? 1 : 0);
            if (operation == graph::Operation::kEventInput && frame.external_.events_)
                for (const auto& event : frame.external_.events_->Events()) {
                    if (event.source_.node_ != node.id_) continue;
                    versions.insert(versions.end(), {event.sequence_, event.generation_,
                                                     std::bit_cast<std::uint64_t>(event.seconds_),
                                                     static_cast<std::uint64_t>(event.kind_),
                                                     std::bit_cast<std::uint64_t>(event.value_)});
                }
            if (plan.beat_grid_) {
                versions.push_back(std::bit_cast<std::uint64_t>(plan.beat_grid_->bpm_));
                versions.push_back(plan.beat_grid_->beats_per_bar_);
                versions.push_back(plan.beat_grid_->beat_unit_);
                versions.push_back(std::bit_cast<std::uint64_t>(plan.beat_grid_->origin_seconds_));
            }
            if (operation == graph::Operation::kEventAudio && frame.external_.audio_) {
                versions.push_back(frame.external_.audio_->generation_);
                versions.push_back(frame.external_.audio_->onset_id_);
                versions.push_back(frame.external_.audio_->valid_ ? 1 : 0);
            }
        }
        const auto external_value = detail::ExternalScalar(instruction, frame);
        if (external_value) versions.push_back(std::bit_cast<std::uint64_t>(*external_value));
        if (operation == graph::Operation::kAudioSpectrum ||
            operation == graph::Operation::kPointInstances ||
            operation == graph::Operation::kSpectrumPoints)
            for (const auto band : detail::SpectrumBands(node, frame.external_))
                versions.push_back(std::bit_cast<std::uint32_t>(band));
        const bool dirty = redraw || !state.node_ || *state.node_ != node ||
                           state.input_versions_ != versions ||
                           (operation == graph::Operation::kFeedback && frame.advance_state_) ||
                           state.target_retired_;
        if (dirty) {
            ++result.evaluated_;
            state.output_.node_ = node.id_;
            state.output_.version_ = next_output_version_++;
            render::DrawList list;
            list.width_ = extent.width_;
            list.height_ = extent.height_;
            if (graph::HasEventReset(operation) && frame.advance_state_ &&
                !instruction.inputs_.empty() && instruction.inputs_.back()) {
                const auto& events = result.outputs_.at(*instruction.inputs_.back()).events_;
                bool reset = false;
                if (events) {
                    auto last = state.last_reset_sequence_;
                    for (const auto& event : events->Events()) {
                        if (event.generation_ != frame.reset_generation_ + 1 ||
                            event.sequence_ <= state.last_reset_sequence_)
                            continue;
                        last = std::max(last, event.sequence_);
                        reset |= event.kind_ != parameters::EventKind::kGate || event.value_ != 0;
                    }
                    state.last_reset_sequence_ = last;
                }
                if (reset) {
                    state.points_.reset();
                    state.gpu_particles_.reset();
                    state.physics_.reset();
                    state.trail_.reset();
                    state.output_.points_generation_ = 0;
                    if (operation == graph::Operation::kFeedback &&
                        state.history_.Handle().device_) {
                        renderer.Submit(state.target_.Handle(), list, 0x000000ff);
                        renderer.Submit(state.history_.Handle(), list, 0x000000ff);
                    }
                }
            }
            if (detail::IsEventOperation(operation)) {
                if (!state.events_) state.events_ = std::make_unique<detail::EventNode>();
                auto event = state.events_->Evaluate(instruction, result.outputs_, frame, plan,
                                                     next_event_sequence_);
                state.output_.scalar_ = event.scalar_;
                state.output_.events_ = std::move(event.events_);
                state.output_.rejected_events_ = event.rejected_;
                state.output_.event_observation_ = event.observation_;
                state.output_.rejected_event_total_ += event.rejected_;
            } else
                switch (operation) {
                    case graph::Operation::kTextureTrail: {
                        if (!state.trail_ || redraw)
                            state.trail_ = std::make_unique<detail::TrailPass>();
                        const auto half_life =
                                instruction.inputs_[1]
                                        ? input(1).scalar_
                                        : graph::Scalar(node, "trail_half_life", 0.5);
                        const detail::TrailSettings settings{
                                std::clamp(half_life, 0.0, 5.0),
                                graph::Scalar(node, "trail_zoom_rate", 0),
                                graph::Scalar(node, "trail_rotation_rate", 0)};
                        state.output_.texture_ =
                                state.trail_->Draw(input(0).texture_, extent, motion.seconds_,
                                                   frame.advance_state_, settings, renderer);
                        break;
                    }
                    case graph::Operation::kTextureVideo: {
                        if (!state.target_.Handle().device_)
                            state.target_ = renderer.CreateTexture(extent, {}, precision);
                        renderer.Submit(state.target_.Handle(), videos_.Draw(node, extent),
                                        0x00000000);
                        state.output_.texture_ = state.target_.Handle();
                        break;
                    }
                    case graph::Operation::kMaterialShader: {
                        if (!input(0).material_)
                            throw std::invalid_argument("runtime.material_input");
                        state.output_.surface_program_ = surfaces_.Bind(
                                instruction, result.outputs_, frame.seconds_, surfaces, renderer);
                        auto material = *input(0).material_;
                        material.surface_node_ = node.id_;
                        state.output_.material_ = material;
                        break;
                    }
                    case graph::Operation::kTextureShader: {
                        if (!state.target_.Handle().device_)
                            state.target_ = acquire(extent, precision);
                        renderer.Submit(state.target_.Handle(),
                                        shaders_.Draw(instruction, result.outputs_, frame.seconds_,
                                                      white_.Handle(), extent, shaders, renderer),
                                        0);
                        state.output_.texture_ = state.target_.Handle();
                        break;
                    }
                    case graph::Operation::kTextureImage:
                    case graph::Operation::kTextureText: {
                        if (!state.target_.Handle().device_)
                            state.target_ = renderer.CreateTexture(extent, {}, precision);
                        renderer.Submit(state.target_.Handle(),
                                        images_.Draw(node, extent, images, renderer), 0x00000000);
                        state.output_.texture_ = state.target_.Handle();
                        break;
                    }
                    case graph::Operation::kGaussianBlur: {
                        if (!state.blur_) state.blur_ = std::make_unique<detail::BlurPass>();
                        const auto value = instruction.inputs_[1]
                                                   ? input(1).scalar_
                                                   : graph::Scalar(node, "blur_radius", 6);
                        // Authoring radius uses pixels at a 720-pixel short edge so
                        // inline previews and portrait/square profiles keep the same look.
                        const auto radius = static_cast<float>(std::clamp(value, 0.0, 32.0)) *
                                            std::min(extent.width_, extent.height_) / 720.0f;
                        state.output_.texture_ = state.blur_->Draw(input(0).texture_, extent,
                                                                   radius, renderer, precision);
                        break;
                    }
                    case graph::Operation::kPathHelix:
                    case graph::Operation::kPathFromPoints:
                    case graph::Operation::kPathResample:
                    case graph::Operation::kGeometryTube:
                        detail::EvaluatePath(instruction, result.outputs_, state.output_);
                        break;
                    case graph::Operation::kGeometryDeform:
                        detail::EvaluateDeformation(instruction, result.outputs_, state.output_);
                        break;
                    case graph::Operation::kGeometryAnimate:
                        detail::EvaluateAnimation(instruction, result.outputs_, state.output_,
                                                  frame.seconds_);
                        break;
                    case graph::Operation::kGeometryMorph:
                        detail::EvaluateMorph(instruction, result.outputs_, state.output_);
                        break;
                    case graph::Operation::kGeometryCube:
                    case graph::Operation::kGeometryTorus:
                    case graph::Operation::kGeometrySphere:
                    case graph::Operation::kGeometryGlb:
                    case graph::Operation::kMaterialUnlit:
                    case graph::Operation::kMaterialPbr:
                    case graph::Operation::kMaterialTextures:
                    case graph::Operation::kDirectionalLight:
                    case graph::Operation::kPointLight:
                    case graph::Operation::kSpotLight:
                    case graph::Operation::kSceneInstance:
                    case graph::Operation::kSceneTransform:
                    case graph::Operation::kSceneMerge:
                    case graph::Operation::kSceneEnvironment:
                    case graph::Operation::kSceneShadow:
                    case graph::Operation::kSceneCamera:
                        detail::EvaluateScene(instruction, result.outputs_, state.output_,
                                              resources);
                        break;
                    case graph::Operation::kGpuParticleEmitter:
                        if (!state.gpu_particles_)
                            state.gpu_particles_ = std::make_unique<detail::GpuParticlePass>();
                        state.output_.gpu_points_ = state.gpu_particles_->Evaluate(
                                instruction, result.outputs_, simulation_frame, renderer);
                        state.output_.gpu_point_capacity_ = static_cast<std::uint32_t>(
                                graph::Scalar(node, "particle_capacity", 65536));
                        break;
                    case graph::Operation::kGpuPointMap:
                        if (!state.gpu_mapping_)
                            state.gpu_mapping_ = std::make_unique<detail::GpuPointMapPass>();
                        state.output_.gpu_points_ = state.gpu_mapping_->Evaluate(
                                instruction, result.outputs_, renderer);
                        state.output_.gpu_point_capacity_ = input(0).gpu_point_capacity_;
                        break;
                    case graph::Operation::kGpuTextureSample: {
                        if (input(0).gpu_sampling_)
                            throw std::invalid_argument("graph.gpu_sample_chain");
                        const auto amount = [&](std::size_t port, std::string_view key,
                                                double fallback) {
                            const auto value = instruction.inputs_[port]
                                                       ? input(port).scalar_
                                                       : graph::Scalar(node, key, fallback);
                            return static_cast<float>(
                                    std::isfinite(value) ? std::clamp(value, 0.0, 1.0) : fallback);
                        };
                        state.output_.gpu_points_ = input(0).gpu_points_;
                        state.output_.gpu_point_capacity_ = input(0).gpu_point_capacity_;
                        state.output_.gpu_sampling_ = render::GpuPointSampling{
                                input(1).texture_, amount(2, "sample_color", 1),
                                amount(3, "sample_size", 0)};
                        break;
                    }
                    case graph::Operation::kGpuPointRender: {
                        if (!state.target_.Handle().device_)
                            state.target_ = acquire(extent, precision);
                        const auto opacity = instruction.inputs_[1]
                                                     ? input(1).scalar_
                                                     : graph::Scalar(node, "opacity", 1);
                        const render::GpuPointStyle style{
                                float(std::isfinite(opacity) ? std::clamp(opacity, 0.0, 1.0) : 1),
                                graph::Scalar(node, "point_blend", 1) == 1, input(0).gpu_sampling_};
                        renderer.SubmitGpuPoints(state.target_.Handle(), input(0).gpu_points_,
                                                 style);
                        state.output_.texture_ = state.target_.Handle();
                        break;
                    }
                    case graph::Operation::kPointInstances:
                        state.output_.scene_ = detail::PointInstances(instruction, result.outputs_,
                                                                      frame.external_);
                        break;
                    case graph::Operation::kSceneCapture: {
                        if (!input(0).scene_) throw std::invalid_argument("runtime.scene_input");
                        if (!state.capture_)
                            state.capture_ = std::make_unique<detail::SceneCapture>();
                        const auto camera =
                                instruction.inputs_[1] ? input(1).camera_.value() : scene::Camera{};
                        state.output_.scene_image_ =
                                state.capture_->Draw(*input(0).scene_, camera, extent, precision,
                                                     renderer, result.outputs_);
                        break;
                    }
                    case graph::Operation::kSceneColor:
                        state.output_.texture_ = input(0).scene_image_.value().color_;
                        break;
                    case graph::Operation::kSceneDepth:
                        state.output_.depth_ = input(0).scene_image_.value().depth_;
                        break;
                    case graph::Operation::kSceneRender: {
                        if (!input(0).scene_) throw std::invalid_argument("runtime.scene_input");
                        if (!state.scene_color_)
                            state.scene_color_ = std::make_unique<detail::SceneColor>();
                        const auto camera =
                                instruction.inputs_[1] ? input(1).camera_.value() : scene::Camera{};
                        state.output_.texture_ = state.scene_color_->Draw(
                                *input(0).scene_, camera, extent, precision,
                                graph::Scalar(node, "scene_antialiasing", 0) == 1, renderer,
                                result.outputs_);
                        break;
                    }
                    case graph::Operation::kSpectrumPoints:
                        state.output_.points_ = detail::SpectrumPoints(instruction, result.outputs_,
                                                                       frame.external_);
                        if (!state.output_.points_generation_ || !state.node_ ||
                            graph::Scalar(*state.node_, "point_count", 128) !=
                                    graph::Scalar(node, "point_count", 128))
                            state.output_.points_generation_ = next_points_generation_++;
                        break;
                    case graph::Operation::kPointGrid:
                        state.output_.points_ = detail::GridPoints(node);
                        state.output_.points_generation_ = next_points_generation_++;
                        break;
                    case graph::Operation::kParticleEmitter:
                        if (!state.points_) state.points_ = std::make_unique<detail::PointState>();
                        if (!state.output_.points_generation_ || !state.node_ ||
                            graph::Scalar(*state.node_, "seed", 1) !=
                                    graph::Scalar(node, "seed", 1) ||
                            (state.points_->last_seconds_ &&
                             motion.seconds_ < *state.points_->last_seconds_))
                            state.output_.points_generation_ = next_points_generation_++;
                        state.output_.points_ = detail::EmitPoints(
                                *state.points_, instruction, result.outputs_, simulation_frame);
                        break;
                    case graph::Operation::kPointTransform:
                        state.output_.points_generation_ = input(0).points_generation_;
                        state.output_.points_ = detail::TransformPoints(
                                instruction, result.outputs_,
                                static_cast<double>(extent.width_) / extent.height_);
                        break;
                    case graph::Operation::kPointPhysics: {
                        if (!state.physics_)
                            state.physics_ = std::make_unique<detail::PointPhysics>();
                        const auto generation = state.physics_->Generation();
                        state.output_.points_ = state.physics_->Evaluate(
                                instruction, result.outputs_, simulation_frame);
                        if (generation != state.physics_->Generation())
                            state.output_.points_generation_ = next_points_generation_++;
                        break;
                    }
                    case graph::Operation::kPointRender: {
                        if (!input(0).points_) throw std::invalid_argument("runtime.points");
                        if (!state.target_.Handle().device_)
                            state.target_ = acquire(extent, precision);
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
                    case graph::Operation::kControlScalar:
                    case graph::Operation::kAudioBand:
                        state.output_.scalar_ = external_value.value();
                        break;
                    case graph::Operation::kMotionPhase: {
                        auto& phase = phases_[node.id_];
                        if (phase.generation_ != motion.generation_) {
                            phase.phase_ = {};
                            phase.generation_ = motion.generation_;
                        }
                        const auto speed = instruction.inputs_[0] ? input(0).scalar_
                                                                  : graph::Scalar(node, "speed", 1);
                        state.output_.scalar_ = phase.phase_.Advance(
                                motion.seconds_, std::clamp(speed, -1e6, 1e6),
                                graph::Scalar(node, "duration", 16), frame.advance_state_);
                        break;
                    }
                    case graph::Operation::kTime:
                    case graph::Operation::kOscillator:
                    case graph::Operation::kSample:
                    case graph::Operation::kConstant:
                    case graph::Operation::kExpression:
                    case graph::Operation::kMath:
                    case graph::Operation::kLocalTime:
                    case graph::Operation::kTimeEnvelope:
                    case graph::Operation::kCurve:
                    case graph::Operation::kMap:
                    case graph::Operation::kCompare:
                    case graph::Operation::kSelect:
                    case graph::Operation::kNoise:
                        state.output_.scalar_ = detail::EvaluateScalar(instruction, result.outputs_,
                                                                       frame.seconds_);
                        break;
                    case graph::Operation::kOutput:
                        state.output_.texture_ = input(0).texture_;
                        break;
                    case graph::Operation::kFeedback:
                        if (!state.target_.Handle().device_) {
                            state.target_ = renderer.CreateTexture(extent, {}, precision);
                            state.history_ = renderer.CreateTexture(extent, {}, precision);
                            renderer.Submit(state.target_.Handle(), list, 0x000000ff);
                            renderer.Submit(state.history_.Handle(), list, 0x000000ff);
                        }
                        state.output_.texture_ = state.history_.Handle();
                        break;
                    case graph::Operation::kVectorFill:
                    case graph::Operation::kVectorStroke:
                        if (!state.target_.Handle().device_)
                            state.target_ = acquire(extent, precision);
                        vectors_.Draw(instruction, result.outputs_, white_.Handle(), list);
                        renderer.Submit(state.target_.Handle(), list, 0);
                        state.output_.texture_ = state.target_.Handle();
                        break;
                    default:
                        if (!state.target_.Handle().device_)
                            state.target_ = acquire(extent, precision);
                        const auto clear =
                                detail::DrawTexture(instruction, result.outputs_, frame.external_,
                                                    white_.Handle(), list);
                        renderer.Submit(state.target_.Handle(), list, clear);
                        state.output_.texture_ = state.target_.Handle();
                        break;
                }
            state.node_ = node;
            state.input_versions_ = std::move(versions);
            state.target_retired_ = false;
        }
        result.outputs_[index] = state.output_;
        result.rejected_events_ += state.output_.rejected_events_;
        result.rejected_event_total_ += state.output_.rejected_event_total_;
        if (frame.profile_nodes_) {
            const auto after = renderer.Stats();
            result.profiles_[index] = {
                    node.id_,
                    std::chrono::duration<double, std::milli>(Clock::now() - profile_start).count(),
                    after.passes_ - before.passes_,
                    static_cast<std::int64_t>(after.texture_bytes_) -
                            static_cast<std::int64_t>(before.texture_bytes_)};
        }
        retire(index);
        if (checkpoint(index + 1, profile_start)) return {};
    }
    // Feedback reads have finished. Write into the other texture, then swap for next frame.
    for (std::size_t index = 0; index < plan.instructions_.size(); ++index) {
        const auto& instruction = plan.instructions_[index];
        if (!lifetimes_.Primary(index) && !frame.evaluate_viewers_) continue;
        if (instruction.operation_ != graph::Operation::kFeedback) continue;
        if (!frame.advance_state_) continue;
        const auto profile_start = std::chrono::steady_clock::now();
        auto& state = states_.at(instruction.node_.id_);
        const auto source = result.outputs_.at(instruction.inputs_.at(0).value()).texture_;
        render::DrawList list;
        list.width_ = state.extent_.width_;
        list.height_ = state.extent_.height_;
        detail::AppendTextureQuad(list, source, 0xffffffff, 0xffffffff);
        renderer.Submit(state.target_.Handle(), list, 0x000000ff);
        std::swap(state.target_, state.history_);
        if (frame.profile_nodes_) {
            ++result.profiles_[index].passes_;
            result.profiles_[index].cpu_ms_ +=
                    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() -
                                                              profile_start)
                            .count();
        }
    }
    retire(plan.instructions_.size());
    targets_.EndFrame();
    result.final_ = result.outputs_.at(plan.output_).texture_;
    if (!frame.advance_state_)
        paused_frame_ =
                PausedFrame{frame, result, lifetimes_.Generation(), presentation_generation};
    return result;
}
}  // namespace rhythm::runtime
