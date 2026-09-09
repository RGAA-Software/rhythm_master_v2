#include <iostream>

#include "rhythm/platform/host.h"

namespace rhythm::validation {
void VerifyEventPixels(render::Renderer& renderer);
}
int main() {
    try {
        rhythm::platform::Host host(true);
        auto renderer = host.CreateRenderer();
        rhythm::validation::VerifyEventPixels(renderer);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
