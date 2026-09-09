#include "audio_test_host.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <stdexcept>

namespace rhythm::audio::test {
void InitializeNativeAudioTest() {
    SDL_SetMainReady();
    if (!SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy"))
        throw std::runtime_error("audio.test_dummy_driver");
}
}  // namespace rhythm::audio::test
