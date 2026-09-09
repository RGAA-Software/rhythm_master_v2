#pragma once

#include <map>
#include <string>

#include "rhythm/parameters/event_track.h"

namespace rhythm::studio {
// Stable action selection and an explicit edit form. A completed form publishes
// one immutable track; typing never repeatedly triggers/recompiles a live graph.
class EventTrackEditor final {
   public:
    std::optional<parameters::EventTrack> Draw(const parameters::EventTrack& track,
                                               const std::string& id, double seconds,
                                               const std::map<std::string, std::string>& text);
    void Reset();

   private:
    std::string id_{};
    std::optional<parameters::EventTrack> previous_{};
    std::optional<parameters::RecordedEvent> selected_{};
    std::string error_{};
};
}  // namespace rhythm::studio
