#include <algorithm>

#include "world_impl.h"

namespace rhythm::physics {
void World::Impl::ReadContacts(StepResult& result) {
    constexpr std::size_t maximum = 4096;
    std::size_t scanned = 0;
    const auto append = [&](ContactKind kind, b2ShapeId first, b2ShapeId second,
                            Vector position = {}, Vector normal = {}, float speed = 0) {
        const auto a = shape_bodies_.find(b2StoreShapeId(first));
        const auto b = shape_bodies_.find(b2StoreShapeId(second));
        if (a == shape_bodies_.end() || b == shape_bodies_.end()) return;
        if (result.contacts_.size() == maximum) {
            result.events_limited_ = true;
            return;
        }
        result.contacts_.push_back({kind, a->second, b->second, position, normal, speed});
    };
    const auto scan_count = [&](int count) {
        const auto available = maximum - scanned;
        const auto bounded = std::min(available, static_cast<std::size_t>(std::max(0, count)));
        scanned += bounded;
        result.events_limited_ |= bounded < static_cast<std::size_t>(std::max(0, count));
        return bounded;
    };
    // The native event arrays are borrowed only during this synchronous call.
    // Copy values before any subsequent step/body mutation invalidates them.
    const auto contacts = b2World_GetContactEvents(native_.Get());
    if (contacts.beginEvents)
        for (std::size_t index = 0, count = scan_count(contacts.beginCount); index < count;
             ++index) {
            const auto& event = contacts.beginEvents[index];
            append(ContactKind::kBegin, event.shapeIdA, event.shapeIdB);
        }
    if (contacts.endEvents)
        for (std::size_t index = 0, count = scan_count(contacts.endCount); index < count; ++index) {
            const auto& event = contacts.endEvents[index];
            append(ContactKind::kEnd, event.shapeIdA, event.shapeIdB);
        }
    if (contacts.hitEvents)
        for (std::size_t index = 0, count = scan_count(contacts.hitCount); index < count; ++index) {
            const auto& event = contacts.hitEvents[index];
            append(ContactKind::kHit, event.shapeIdA, event.shapeIdB, detail::Value(event.point),
                   detail::Value(event.normal), event.approachSpeed);
        }
    const auto sensors = b2World_GetSensorEvents(native_.Get());
    if (sensors.beginEvents)
        for (std::size_t index = 0, count = scan_count(sensors.beginCount); index < count;
             ++index) {
            const auto& event = sensors.beginEvents[index];
            append(ContactKind::kSensorBegin, event.sensorShapeId, event.visitorShapeId);
        }
    if (sensors.endEvents)
        for (std::size_t index = 0, count = scan_count(sensors.endCount); index < count; ++index) {
            const auto& event = sensors.endEvents[index];
            append(ContactKind::kSensorEnd, event.sensorShapeId, event.visitorShapeId);
        }
}
}  // namespace rhythm::physics
