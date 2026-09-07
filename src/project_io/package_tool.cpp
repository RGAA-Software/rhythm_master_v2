#include <iostream>

#include "rhythm/project/package.h"
#include "rhythm/project/store.h"

// Command-line argument pointers are borrowed only at the process ABI boundary.
#ifdef _WIN32
int wmain(int argc, wchar_t* argv[]) {
#else
int main(int argc, char* argv[]) {
#endif
    try {
        if (argc != 3)
            throw std::invalid_argument(
                    "Usage: rhythm_package <project-or-template-directory> <output.rhythmpack>");
        const std::filesystem::path input(argv[1]);
        const std::filesystem::path output(argv[2]);
        const auto loaded = std::filesystem::exists(input / "CURRENT")
                                    ? rhythm::project::Load(input)
                                    : rhythm::project::LoadRevision(input);
        rhythm::project::PublishSnapshot(output, loaded.snapshot_, input / "assets");
        const auto verified = rhythm::project::LoadPackage(output);
        std::cout << "Published runtime ABI 2, " << verified.program_.instructions_.size()
                  << " instructions, " << verified.assets_.size() << " assets\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
