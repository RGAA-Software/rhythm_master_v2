#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

#include "rhythm/player/package_loader.h"
#include "rhythm/player/session.h"

int main(int argc, char* argv[]) {
    using namespace rhythm;
    try {
        const auto check = [](bool value) {
            if (!value) throw std::runtime_error("player.loader_contract");
        };
        const auto directory =
                argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::path("loader-tests");
        std::filesystem::create_directories(directory);
        graph::Registry registry;
        graph::Document graph;
        graph.id_ = "loader.test";
        graph.output_ = 2;
        graph.nodes_ = {registry.MakeNode(1, "texture.gradient"),
                        registry.MakeNode(2, "output.texture")};
        graph.edges_ = {{1, 1, 2, "source"}};
        const auto source = directory / "source.rhythmpack";
        const auto installed = directory / "installed.rhythmpack";
        project::PublishPackage(source, graph, "new");
        project::PublishPackage(installed, graph, "old");
        player::PackageLoader loader;
        const auto take = [&] {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
            while (std::chrono::steady_clock::now() < deadline) {
                if (auto result = loader.Take()) return std::move(*result);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            throw std::runtime_error("player.loader_timeout");
        };
        check(!loader.Take() && loader.StartFile(source, installed));
        check(loader.Busy() && !loader.StartFile(source));
        auto loaded = take();
        check(!loader.Busy() && loaded.error_ == player::PackageLoadError::kNone &&
              loaded.package_);
        check(project::LoadPackage(installed).title_ == "new" && std::filesystem::exists(source));
        player::Session session;
        session.LoadPrepared(std::move(*loaded.package_));
        check(session.Title() == "new");
        const auto invalid = directory / "invalid.rhythmpack";
        {
            std::ofstream output(invalid, std::ios::binary);
            output << "invalid";
        }
        check(loader.StartFile(invalid, installed));
        check(take().error_ == player::PackageLoadError::kInvalid);
        check(project::LoadPackage(installed).title_ == "new" && session.Title() == "new");
        check(loader.StartFile(directory / "missing.rhythmpack"));
        check(take().error_ == player::PackageLoadError::kRead);
        const auto huge = directory / "oversized.rhythmpack";
        {
            std::ofstream output(huge, std::ios::binary);
            output.seekp(project::kMaximumFilePackageBytes);
            output.put('\0');
        }
        check(loader.StartFile(huge));
        check(take().error_ == player::PackageLoadError::kRead);
        for (int cycle = 0; cycle < 10; ++cycle) {
            check(loader.StartFile(source));
            auto result = take();
            check(result.package_ && result.package_->Ready());
        }
        check(loader.StartFile(source, installed));
        loader.Cancel();
        auto cancelled = take();
        check(cancelled.error_ == player::PackageLoadError::kCancelled ||
              (cancelled.error_ == player::PackageLoadError::kNone && cancelled.package_));
        check(project::LoadPackage(installed).title_ == "new");
        {
            player::PackageLoader departing;
            check(departing.StartFile(source, installed));
        }
        check(project::LoadPackage(installed).title_ == "new");
        std::cout << "package loader contracts passed: bounded worker, prepare/install, "
                     "bad-package retention, size limit, recovery, cancel/exit\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
