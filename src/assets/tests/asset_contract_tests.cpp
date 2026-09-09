#include <array>
#include <chrono>
#include <future>
#include <iostream>
#include <stdexcept>
#include <thread>

#include "rhythm/assets/importer.h"
#include "rhythm/assets/store.h"
#include "rhythm/storage/atomic_file.h"
#include "rhythm/storage/file_bytes.h"

namespace {
void Check(bool value) {
    if (!value) throw std::runtime_error("asset.contract");
}
rhythm::assets::ImportResult Await(rhythm::assets::Importer& importer) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline) {
        if (auto result = importer.Take()) return *result;
        std::this_thread::yield();
    }
    throw std::runtime_error("asset.worker_timeout");
}
template <class Function>
void Reject(Function function) {
    bool rejected = false;
    try {
        function();
    } catch (const std::exception&) {
        rejected = true;
    }
    Check(rejected);
}
}  // namespace
int main(int argc, char* argv[]) {
    using namespace rhythm;
    try {
        const auto root =
                argc == 2 ? std::filesystem::path(argv[1]) : std::filesystem::path("asset-tests");
        assets::Store store(root / "assets");
        const auto source = root / std::filesystem::path(u8"测试素材.bin");
        storage::WriteDurable(source, "abc");
        const auto asset = store.Import(source, "application/octet-stream");
        Check(asset.id_.sha256_ ==
                      "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" &&
              asset.bytes_ == 3);
        Check(store.Read(asset) == "abc" && store.Verify(asset));
        Check(store.OpenId(asset.id_, 3).Size() == 3);
        Reject([&] { store.OpenId(asset.id_, 2); });
        Reject([&] { store.OpenId({"../outside"}, 3); });
        {
            const auto opened = store.Open(asset, 3);
            std::array<std::uint8_t, 3> bytes{};
            Check(opened.Read(0, bytes) == 3 && bytes[0] == 'a' && bytes[2] == 'c' &&
                  assets::VerifyFile(asset, opened));
            auto wrong = asset;
            wrong.id_.sha256_[0] = '0';
            Check(!assets::VerifyFile(wrong, opened));
            Reject([&] { store.Open(asset, 2); });
            std::stop_source canceled;
            canceled.request_stop();
            Reject([&] { store.Open(asset, 3, canceled.get_token()); });
        }
        assets::Store copied(root / "copied-assets");
        Check(copied.CopyFrom(store, asset, 3) == asset && copied.Read(asset) == "abc");
        Reject([&] { copied.CopyFrom(store, asset, 2); });
        {
            assets::Importer importer;
            Check(importer.Start(root / "assets", source, "application/octet-stream"));
            Check(!importer.Start(root / "assets", source, "application/octet-stream"));
            std::optional<assets::ImportResult> result;
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            while (!result && std::chrono::steady_clock::now() < deadline) {
                result = importer.Take();
                std::this_thread::yield();
            }
            Check(result && result->error_.empty() && result->asset_ == asset && !importer.Busy());
            // A failed operation must not poison the reusable worker or its future.
            Check(importer.Start(root / "assets", root / "missing.bin",
                                 "application/octet-stream"));
            Check(!Await(importer).error_.empty() && !importer.Busy());
            for (int index = 0; index < 16; ++index) {
                Check(importer.Start(root / "assets", source, "application/octet-stream"));
                Check(Await(importer).asset_ == asset && !importer.Busy());
            }
        }
        const auto blob = root / "assets/sha256/ba" / asset.id_.sha256_;
        const auto timestamp = std::filesystem::last_write_time(blob);
        Check(store.Import(source, "application/octet-stream") == asset);
        Check(std::filesystem::last_write_time(blob) == timestamp);
        Reject([&] { store.Import(source, "application/octet-stream", 2); });
        std::stop_source cancelled;
        cancelled.request_stop();
        Reject([&] { store.Import(source, "application/octet-stream", 3, cancelled.get_token()); });
        Reject([&] { store.Read({{"../outside"}, 3, "text/plain"}); });
        Reject([&] { store.Read(asset, 2); });
        storage::WriteDurable(source, "different source bytes");
        Check(store.Read(asset) == "abc");
        storage::WriteDurable(blob, "bad");
        Check(!store.Verify(asset));
        Reject([&] { store.OpenId(asset.id_, 3); });
        Reject([&] { store.Open(asset); });
        Check(copied.Read(asset) == "abc");
        Reject([&] { copied.CopyFrom(store, asset); });
        Reject([&] { store.Read(asset); });
        Check(store.Inspect(asset) == assets::AssetHealth::kCorrupt);
        const auto repair = root / "repair.bin";
        storage::WriteDurable(repair, "xyz");
        Reject([&] { store.Restore(repair, asset); });
        Check(!store.Verify(asset));
        storage::WriteDurable(repair, "abc");
        Reject([&] { store.Restore(repair, asset, cancelled.get_token()); });
        Check(!store.Verify(asset));
        store.Restore(repair, asset);
        Check(store.Read(asset) == "abc" && store.Inspect(asset) == assets::AssetHealth::kValid);
        {
            assets::Importer maintenance;
            Check(maintenance.StartInspect(root / "assets", {asset}));
            auto review = Await(maintenance);
            Check(review.error_.empty() && review.checks_.size() == 1 &&
                  review.checks_[0].health_ == assets::AssetHealth::kValid);
            storage::WriteDurable(blob, "bad");
            Check(maintenance.StartInspect(root / "assets", {asset}));
            Check(Await(maintenance).checks_[0].health_ == assets::AssetHealth::kCorrupt);
            Check(maintenance.StartRestore(root / "assets", repair, asset));
            Check(Await(maintenance).restored_ == asset && store.Verify(asset));
            Check(!maintenance.StartInspect(root / "assets",
                                            std::vector<assets::AssetRecord>(65, asset)));
        }
        const auto restored_time = std::filesystem::last_write_time(blob);
        store.Restore(repair, asset);
        Check(std::filesystem::last_write_time(blob) == restored_time);
        std::filesystem::remove(blob);
        Check(store.Inspect(asset) == assets::AssetHealth::kMissing);
        store.Restore(repair, asset);
        Check(store.Read(asset) == "abc");
        {
            const auto large = root / "cancel.bin";
            storage::WriteDurable(large, std::string(8 * 1024 * 1024, 'x'));
            {
                assets::Importer importer;
                Check(importer.Start(root / "shutdown-assets", large, "application/octet-stream"));
                // Immediate destruction must cancel/join even a still-queued job.
            }
            for (const auto& file : std::filesystem::directory_iterator(root / "shutdown-assets"))
                Check(file.path().extension() != ".tmp");
            // Acquiring the writer again proves destruction released native I/O ownership.
            Check(assets::Store(root / "shutdown-assets")
                          .Import(source, "application/octet-stream")
                          .bytes_ == 22);
            std::stop_source stop;
            auto worker = std::async(std::launch::async, [&] {
                try {
                    store.Import(large, "application/octet-stream", 8 * 1024 * 1024,
                                 stop.get_token());
                    return false;
                } catch (const std::exception& error) {
                    if (std::string_view(error.what()) != "asset.cancelled") throw;
                    return true;
                }
            });
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            bool copying = false;
            while (!copying && std::chrono::steady_clock::now() < deadline &&
                   worker.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
                for (const auto& file : std::filesystem::directory_iterator(root / "assets"))
                    copying |= file.path().extension() == ".tmp";
                std::this_thread::yield();
            }
            stop.request_stop();
            Check(worker.get() && copying && store.Verify(asset));
        }
        for (const auto& file : std::filesystem::directory_iterator(root / "assets"))
            Check(file.path().extension() != ".tmp");
        std::cout << "asset contracts passed: hash, dedup, atomic import, limits, cancellation, "
                     "corruption\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
