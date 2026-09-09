#pragma once

namespace rhythm::audio::test {
// Standalone Android test executables have no Java/SDL activity entry point.
// Explicit dummy device selection is a test-only host bootstrap, not a player
// backend fallback or evidence of hardware output.
void InitializeNativeAudioTest();
}  // namespace rhythm::audio::test
