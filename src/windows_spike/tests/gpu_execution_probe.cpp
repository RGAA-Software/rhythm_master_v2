#include "gpu_execution_probe.h"

#include <array>
#include <iostream>

#include "rhythm/platform/host.h"
int main(int argc, char* argv[]) {
    try {
        std::array<std::uint8_t, 32 * 16 * 4> pixels{};
        rhythm::platform::Host host(true);
        auto renderer = host.CreateRenderer();
        if (argc == 2 && std::string_view(argv[1]) == "--morph") {
            rhythm::validation::VerifyMeshMorph(renderer);
#if defined(RHYTHM_MODEL_IMAGE_PROBE)
            rhythm::validation::VerifyModelMorph(renderer);
#endif
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--skinning") {
            rhythm::validation::VerifyMeshSkinning(renderer);
#if defined(RHYTHM_MODEL_IMAGE_PROBE)
            rhythm::validation::VerifyModelSkin(renderer);
#endif
            return 0;
        }
        if (argc == 3 && std::string_view(argv[1]) == "--image-program") {
            rhythm::validation::VerifyImageProgram(renderer, std::filesystem::path(argv[2]));
            return 0;
        }
        rhythm::validation::VerifyGpuExecution(pixels);
        rhythm::validation::VerifySceneInstances(renderer);
        rhythm::validation::VerifyGpuParticles(renderer);
        rhythm::validation::VerifyColorPipeline(renderer);
        rhythm::validation::VerifySampleableDepth(renderer);
        rhythm::validation::VerifyPositionalLights(renderer);
        rhythm::validation::VerifyMaterialTextures(renderer);
        rhythm::validation::VerifySceneShadows(renderer);
        rhythm::validation::VerifyEnvironmentLighting(renderer);
        rhythm::validation::VerifyMeshDeformation(renderer);
        rhythm::validation::VerifyMeshSkinning(renderer);
        rhythm::validation::VerifyMeshMorph(renderer);
#if defined(RHYTHM_MODEL_IMAGE_PROBE)
        rhythm::validation::VerifyModelImages(renderer);
        rhythm::validation::VerifyModelSkin(renderer);
        rhythm::validation::VerifyModelMorph(renderer);
#endif
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
