#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include "rhythm/storage/atomic_file.h"

int main(int argc, char* argv[]) {
    using namespace rhythm::storage;
    try {
        if (argc != 2) throw std::invalid_argument("publication_tests output");
        const auto root =
                std::filesystem::path(argv[1]) /
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        std::filesystem::create_directories(root);
        WriteDurable(root / "first", "finished");
        PublishNew(root / "first", root / "output");
        WriteDurable(root / "second", "replacement");
        bool rejected = false;
        try {
            PublishNew(root / "second", root / "output");
        } catch (const std::exception&) {
            rejected = true;
        }
        std::ifstream input(root / "output");
        std::string content;
        input >> content;
        if (!rejected || content != "finished" || !std::filesystem::exists(root / "second"))
            throw std::runtime_error("publication replaced or consumed an existing file");
        std::cout << "atomic new-file publication preserves existing destinations\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
