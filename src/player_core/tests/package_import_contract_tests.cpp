#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

#include "package_imports.h"
#include "rhythm/player/session.h"

int main(int argc, char* argv[]) {
    using namespace rhythm;
    try {
        const auto check = [](bool value) {
            if (!value) throw std::runtime_error("player.import_contract");
        };
        const auto root = std::filesystem::absolute(
                argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::path("import-tests"));
        const auto cache = root / "cache";
        std::filesystem::create_directories(cache);
        const auto selected = root / "selected.rhythmpack";
        graph::Registry registry;
        graph::Document graph;
        graph.id_ = "import.test";
        graph.output_ = 2;
        graph.nodes_ = {registry.MakeNode(1, "texture.gradient"),
                        registry.MakeNode(2, "output.texture")};
        graph.edges_ = {{1, 1, 2, "source"}};
        const auto first = cache / "incoming-first.rhythmpack";
        const auto dropped = cache / "incoming-dropped.rhythmpack";
        const auto latest = cache / "incoming-latest.rhythmpack";
        const auto outside = root / "incoming-outside.rhythmpack";
        const auto ordinary = cache / "ordinary.rhythmpack";
        for (const auto& path : {first, dropped, latest, outside, ordinary})
            project::PublishPackage(path, graph, path.stem().string());
        android_host::PackageImports imports(selected, cache);
        check(!imports.Request(outside) && !imports.Request(ordinary));
        check(imports.Request(first) && !imports.Request(first));
        check(imports.Request(dropped) && imports.Request(latest));
        check(!std::filesystem::exists(dropped));
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        player::Session session;
        int completed = 0;
        while (imports.Busy() && std::chrono::steady_clock::now() < deadline) {
            if (auto result = imports.Take()) {
                check(result->package_.has_value());
                session.LoadPrepared(std::move(*result->package_));
                ++completed;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        check(!imports.Busy() && completed == 2 && session.Title() == "incoming-latest");
        check(project::LoadPackage(selected).title_ == "incoming-latest");
        check(!std::filesystem::exists(first) && !std::filesystem::exists(latest));
        check(std::filesystem::exists(outside) && std::filesystem::exists(ordinary));
        const auto exiting = cache / "incoming-exit.rhythmpack";
        project::PublishPackage(exiting, graph, "exit");
        {
            android_host::PackageImports leaving(selected, cache);
            check(leaving.Request(exiting));
        }
        check(!std::filesystem::exists(exiting));
        check(project::LoadPackage(selected).title_ == "incoming-latest" ||
              project::LoadPackage(selected).title_ == "exit");
        check(std::filesystem::exists(outside) && std::filesystem::exists(ordinary));
        std::cout << "package import contracts passed: private cache boundary, active/latest "
                     "queue, worker-before-file cleanup, exit\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
