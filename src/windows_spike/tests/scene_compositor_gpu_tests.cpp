#include <iostream>

#include "rhythm/platform/host.h"
#include "scene_compositor_probe.h"

// Process arguments are borrowed only at this synchronous native boundary.
int main(int argc, char* argv[]) {
    try {
        rhythm::platform::Host host(true);
        auto renderer = host.CreateRenderer();
        if (argc == 3 && std::string_view(argv[1]) == "--replacement")
            rhythm::validation::VerifySceneReplacement(renderer, argv[2]);
        else if (argc == 3)
            rhythm::validation::VerifySceneDeck(renderer, argv[1], argv[2]);
        else
            rhythm::validation::VerifySceneCompositor(renderer);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
