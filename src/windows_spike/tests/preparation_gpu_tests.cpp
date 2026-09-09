#include <iostream>

#include "preparation_gpu_contracts.h"
#include "rhythm/platform/host.h"

int main() {
    try {
        rhythm::platform::Host host(true);
        auto renderer = host.CreateRenderer();
        rhythm::validation::VerifyPreparedPixels(renderer);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
