#include <algorithm>
#include <chrono>
#include <iostream>
#include <limits>
#include <source_location>
#include <stdexcept>

#include "rhythm/parameters/events.h"

namespace {
void Check(bool value, std::source_location location = std::source_location::current()) {
    if (!value) throw std::runtime_error("event.contract:" + std::to_string(location.line()));
}
}  // namespace
int main() {
    using namespace rhythm::parameters;
    try {
        EventQueue queue;
        Event event{0.5, {0, 1, EventOrigin::kBeat}, 1, 1, EventKind::kPulse, 1};
        Check(queue.Submit(event) == EventAdmission::kWrongGeneration);
        queue.Reset(1, 0);
        Check(queue.Submit(event) == EventAdmission::kAccepted);
        Check(queue.Submit(event) == EventAdmission::kDuplicateOrOld);
        auto second = event;
        second.source_.node_ = 2;
        second.seconds_ = 0.25;
        Check(queue.Submit(second) == EventAdmission::kAccepted);
        Check(queue.Drain(0.24).batch_.Events().empty());
        Check(queue.Drain(0.25, true).due_remaining_ == 1);
        const auto resumed = queue.Drain(0.25);
        Check(resumed.batch_.Events().size() == 1 && resumed.batch_.Events()[0] == second);
        Check(queue.Drain(0.25).batch_.Events().empty());
        Check(queue.Submit(second) == EventAdmission::kDuplicateOrOld);
        Check(queue.Drain(0.5).batch_.Events()[0] == event);
        second.sequence_ = 2;
        Check(queue.Submit(second) == EventAdmission::kLate);
        second.seconds_ = 0.6;
        second.value_ = std::numeric_limits<double>::quiet_NaN();
        Check(queue.Submit(second) == EventAdmission::kInvalid);
        second.value_ = 0.5;
        second.kind_ = EventKind::kGate;
        Check(queue.Submit(second) == EventAdmission::kInvalid);
        second.value_ = 1;
        Check(queue.Submit(second) == EventAdmission::kAccepted);
        queue.Reset(2, 0);
        Check(queue.Pending() == 0 && queue.Submit(event) == EventAdmission::kWrongGeneration);
        event.generation_ = 2;
        Check(queue.Submit(event) == EventAdmission::kAccepted);
        try {
            queue.Reset(0, 0);
            Check(false);
        } catch (const std::invalid_argument&) {
        }
        Check(queue.Pending() == 1);
        try {
            queue.Drain(-1);
            Check(false);
        } catch (const std::invalid_argument&) {
        }
        Check(queue.Pending() == 1);

        // Reversed producers and interleaved scopes must have deterministic
        // same-time order, independent of callback/host arrival order.
        queue.Reset(3, 0);
        for (int source = 128; source > 0; --source) {
            event = {1,
                     {static_cast<std::uint64_t>(source % 2), static_cast<std::uint64_t>(source),
                      EventOrigin::kManual},
                     1,
                     3,
                     EventKind::kPulse,
                     1};
            Check(queue.Submit(event) == EventAdmission::kAccepted);
        }
        event.source_.node_ = 129;
        Check(queue.Submit(event) == EventAdmission::kSourceLimit);
        auto ordered = queue.Drain(1);
        Check(ordered.batch_.Events().size() == 128);
        Check(std::is_sorted(ordered.batch_.Events().begin(), ordered.batch_.Events().end(),
                             EventBefore));

        queue.Reset(4, 0);
        for (std::uint64_t sequence = 1; sequence <= EventQueue::kCapacity; ++sequence) {
            event = {1, {0, 1, EventOrigin::kAudio}, sequence, 4, EventKind::kPulse, 1};
            Check(queue.Submit(event) == EventAdmission::kAccepted);
        }
        ++event.sequence_;
        Check(queue.Submit(event) == EventAdmission::kQueueFull);
        Check(queue.Drain(1, true).due_remaining_ == 1024);
        auto batch = queue.Drain(1);
        Check(batch.batch_.Events().size() == 256 && batch.due_remaining_ == 768);
        Check(batch.batch_.Events().front().sequence_ == 1 &&
              batch.batch_.Events().back().sequence_ == 256);
        // Full-queue rejection did not consume sequence 1025.
        Check(queue.Submit(event) == EventAdmission::kAccepted);
        std::uint64_t last = 256;
        while (queue.Pending()) {
            batch = queue.Drain(1);
            for (const auto& item : batch.batch_.Events()) Check(item.sequence_ == ++last);
        }
        Check(last == 1025);

        const auto start = std::chrono::steady_clock::now();
        for (std::uint64_t run = 5; run < 205; ++run) {
            queue.Reset(run, 0);
            for (std::uint64_t sequence = 1; sequence <= 8; ++sequence)
                for (std::uint64_t node = 128; node > 0; --node)
                    Check(queue.Submit({1,
                                        {0, node, EventOrigin::kOperator},
                                        sequence,
                                        run,
                                        EventKind::kPulse,
                                        1}) == EventAdmission::kAccepted);
            while (queue.Pending()) queue.Drain(1);
        }
        const auto elapsed =
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
                        .count();
        std::cout << "Event identity, generation, stable order, admission and bounded dispatch "
                     "passed; "
                  << "1024 events x 200 (128 interleaved sources), mean " << elapsed / 200
                  << " ms\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
