#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

#include "program_bridge.h"
#include "rhythm/project/package.h"

namespace {
void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
}  // namespace
int main(int argc, char** argv) {
    using namespace rhythm;
    try {
        if (argc != 2) throw std::invalid_argument("test directory required");
        const auto directory =
                std::filesystem::path(argv[1]) /
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        std::filesystem::create_directories(directory / "cache");
        const auto cache = std::filesystem::canonical(directory / "cache");
        const auto source = cache / "incoming-program.rhythmpack";
        const auto rejected = cache / "incoming-rejected.rhythmpack";
        graph::Registry registry;
        graph::Document document;
        document.id_ = "program-import-lifetime";
        document.nodes_ = {registry.MakeNode(1, "texture.gradient"),
                           registry.MakeNode(2, "output.texture")};
        document.edges_ = {{1, 1, 2, "source"}};
        document.output_ = 2;
        project::PublishPackage(source, document, "Owned program import");
        project::PublishPackage(rejected, document, "Rejected copy");
        const auto outside = directory / "outside.rhythmpack";
        project::PublishPackage(outside, document, "Outside source");
        std::optional<android_host::ImportFile> owned;
        player::PerformanceProgram program(directory / "program", {}, {});
        android_host::ProgramCommand command;
        command.action_ = 10;
        command.path_ = source.string();
        const auto apply = [&](const auto& request) {
            return android_host::ApplyProgramCommand(request, program, {}, cache, owned);
        };
        Check(apply(command) && owned && program.Busy(), "import not admitted");
        Check(!apply(command) && std::filesystem::exists(source),
              "duplicate rejected by deleting active source");
        command.path_ = rejected.string();
        Check(!apply(command) && !std::filesystem::exists(rejected), "busy rejected copy leaked");
        command.path_ = outside.string();
        Check(!apply(command) && std::filesystem::exists(outside), "external source claimed");
        command.path_ = (cache / "incoming-missing.rhythmpack").string();
        Check(!apply(command), "missing source accepted");
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (program.Busy() && std::chrono::steady_clock::now() < deadline) {
            program.Pump();
            std::this_thread::yield();
        }
        Check(!program.Busy() && program.Status().error_.empty() &&
                      program.Draft().Entries().size() == 1,
              "pending import damaged by rejected command");
        owned.reset();
        Check(!std::filesystem::exists(source), "completed private source retained");
        const auto& work = program.Draft().Entries().front().work_;
        Check(project::ReadPackage(player::WorkLibrary(directory / "program/works").Open(work))
                              .title_ == "Owned program import",
              "import source cleanup broke persistent work");
        std::cout << "Android program import ownership, duplicate/busy/missing rejection and "
                     "persistence passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
