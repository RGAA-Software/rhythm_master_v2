#include "gpu_execution_probe.h"

#include <array>
#include <iostream>

#include "rhythm/platform/host.h"
int main() {
    try {
        std::array<std::uint8_t, 32 * 16 * 4> pixels{};
        rhythm::platform::Host host(true);
        auto renderer = host.CreateRenderer();
        rhythm::validation::VerifyGpuExecution(pixels);
        rhythm::validation::VerifySceneInstances(renderer);
        rhythm::validation::VerifyGpuParticles(renderer);
        rhythm::validation::VerifyColorPipeline(renderer);
        rhythm::validation::VerifySampleableDepth(renderer);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
