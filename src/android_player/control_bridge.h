#pragma once

#include "rhythm/parameters/control_sequence.h"

namespace rhythm::android_host {
// Render-thread publication and snapshot read. JNI edits are validated under
// the adapter mutex; no Java object or render resource crosses the boundary.
void PublishControls(const parameters::ControlBank& bank,
                     const std::optional<parameters::ControlSequence>& sequence = {});
void PublishControlTime(double seconds);
parameters::ControlValues CurrentControls();
}  // namespace rhythm::android_host
