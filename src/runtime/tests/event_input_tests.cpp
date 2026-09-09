#include <array>
#include <iostream>
#include <source_location>
#include <stdexcept>
#include <string>

#include "rhythm/runtime/runtime.h"

namespace {
void Check(bool value, std::source_location location = std::source_location::current()) {
    if (!value) throw std::runtime_error("event.input:" + std::to_string(location.line()));
}
}  // namespace
int main() {
    using namespace rhythm;
    using namespace parameters;
    try {
        graph::Registry registry;
        graph::Document document;
        document.id_ = "recorded-actions";
        document.nodes_ = {registry.MakeNode(1, "event.input"), registry.MakeNode(2, "event.step"),
                           registry.MakeNode(3, "texture.gradient"),
                           registry.MakeNode(4, "output.texture")};
        document.nodes_[0].properties_["actions"] = EventTrack({{1, 0, EventKind::kPulse, 1},
                                                                {2, 0.5, EventKind::kPulse, 1},
                                                                {3, 1, EventKind::kReset, 1},
                                                                {4, 1.5, EventKind::kPulse, 1}});
        document.edges_ = {{1, 1, 2, "events"}, {2, 2, 3, "amount"}, {3, 3, 4, "source"}};
        document.output_ = 4;
        auto renderer = render::Renderer::CreateNull();
        runtime::Runtime runtime;
        runtime::FrameContext context;
        const auto evaluate = [&](double seconds) {
            context.seconds_ = seconds;
            const auto plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
            renderer.BeginFrame();
            auto result = runtime.Evaluate(plan, context, renderer);
            renderer.EndFrame();
            return result;
        };
        const auto output = [](const runtime::FrameResult& frame, graph::NodeId id) {
            for (const auto& value : frame.outputs_)
                if (value.node_ == id) return value;
            throw std::runtime_error("missing input output");
        };
        for (const int rate : {30, 60, 144}) {
            runtime.Reset();
            ++context.reset_generation_;
            context.advance_state_ = false;
            Check(!output(evaluate(0), 1).events_);
            context.advance_state_ = true;
            for (int frame = 0; frame <= 2 * rate; ++frame) {
                const auto result = evaluate(double(frame) / rate);
                const auto expected = frame < rate / 2       ? 1
                                      : frame < rate         ? 2
                                      : frame < 3 * rate / 2 ? 0
                                                             : 1;
                Check(output(result, 2).scalar_ == expected && !result.rejected_events_);
            }
            Check(output(evaluate(2), 1).event_observation_->count_ == 4);
        }
        // Explicit seek establishes a new baseline, not historical catch-up.
        ++context.reset_generation_;
        Check(output(evaluate(1.25), 2).scalar_ == 0);
        Check(output(evaluate(1.5), 2).scalar_ == 1);
        EventQueue queue;
        queue.Reset(context.reset_generation_ + 1, 1.5);
        EventRecorder recorder;
        recorder.Begin({}, {0, 1, EventOrigin::kManual}, context.reset_generation_ + 1, 1.5);
        Check(queue.Submit({1.5,
                            {0, 1, EventOrigin::kManual},
                            1,
                            context.reset_generation_ + 1,
                            EventKind::kPulse,
                            0.7}) == EventAdmission::kAccepted);
        context.external_.events_ = std::make_shared<const EventBatch>(queue.Drain(1.6).batch_);
        auto result = evaluate(1.6);
        Check(output(result, 2).scalar_ == 2);
        const auto dispatched = output(result, 1).events_->Events()[0];
        Check(dispatched.seconds_ == 1.6 && dispatched.value_ == 0.7);
        Check(recorder.Capture(dispatched) == RecordingAdmission::kRecorded);
        Check(output(evaluate(1.6), 2).scalar_ == 2);
        Check(output(evaluate(1.7), 2).scalar_ == 2);
        context.external_.events_.reset();
        Check(!output(evaluate(1.7), 1).events_);
        const auto recorded = recorder.Finish();
        Check(recorded.Events().size() == 1 && recorded.Events()[0].seconds_ == 1.6);
        document.nodes_[0].properties_["actions"] = recorded;
        ++context.reset_generation_;
        evaluate(0);
        Check(output(evaluate(1.59), 2).scalar_ == 0);
        Check(output(evaluate(1.6), 2).scalar_ == 1);
        Check(output(evaluate(1.7), 2).scalar_ == 1);
        // Same-time new input invalidates the cache; duplicate input does not
        // mutate state. An old generation is reported instead of replayed.
        EventBatch live;
        Check(live.Append({1.7,
                           {0, 1, EventOrigin::kManual},
                           2,
                           context.reset_generation_ + 1,
                           EventKind::kReset,
                           1}));
        context.external_.events_ = std::make_shared<const EventBatch>(live);
        context.advance_state_ = false;
        Check(output(evaluate(1.7), 2).scalar_ == 1);
        context.advance_state_ = true;
        Check(output(evaluate(1.7), 2).scalar_ == 0);
        ++context.reset_generation_;
        Check(evaluate(1.7).rejected_events_ == 1);
        context.external_.events_.reset();
        std::vector<RecordedEvent> dense;
        for (std::uint64_t id = 1; id <= 4096; ++id)
            dense.push_back({id, 0.5, EventKind::kPulse, 1});
        document.nodes_[0].properties_["actions"] = EventTrack(std::move(dense));
        ++context.reset_generation_;
        evaluate(0);
        result = evaluate(0.5);
        Check(output(result, 1).events_->Events().size() == 256);
        Check(result.rejected_events_ == 4096 - 256);
        Check(!output(evaluate(0.6), 1).events_);
        std::cout << "Recorded actions, live dispatch, pause, seek, replay and budgets passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
