#include <picosha2.h>

#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

#include "rhythm/assets/store.h"
#include "rhythm/model_assets/prepare.h"
#include "rhythm/prepared_assets/loader.h"

namespace {
void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
template <typename F>
void Reject(F action) {
    bool rejected = false;
    try {
        action();
    } catch (const std::exception&) {
        rejected = true;
    }
    Check(rejected, "invalid model resource accepted");
}
}  // namespace
int main(int argc, char** argv) {
    using namespace rhythm;
    try {
        Check(argc == 3, "fixture and scratch paths required");
        std::ifstream file(argv[1], std::ios::binary);
        Check(bool(file), "fixture unavailable");
        const std::string bytes{std::istreambuf_iterator<char>(file), {}};
        project::PackagedAsset asset{{{picosha2::hash256_hex_string(bytes.begin(), bytes.end())},
                                      bytes.size(),
                                      "model/gltf-binary"},
                                     bytes};
        graph::Registry registry;
        graph::Document document;
        document.id_ = "model.asset.test";
        document.nodes_ = {
                registry.MakeNode(1, "geometry.glb"), registry.MakeNode(2, "scene.instance"),
                registry.MakeNode(3, "scene.render"), registry.MakeNode(4, "output.texture")};
        document.nodes_[0].properties_["asset"] = asset.record_.id_;
        document.edges_ = {{1, 1, 2, "geometry"}, {2, 2, 3, "scene"}, {3, 3, 4, "source"}};
        document.output_ = 4;
        const auto plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
        const std::vector<project::PackagedAsset> assets{asset};
        const auto resources = model_assets::Prepare(plan, assets);
        Check(resources->models_.size() == 1 && resources->models_[0].vertices_ == 24 &&
                      resources->models_[0].indices_ == 36 && resources->models_[0].draws_ == 1,
              "upstream Cesium Box geometry counts");
        Check(model_assets::Covers(plan, *resources), "prepared catalog coverage");
        Check(!model_assets::Covers(plan, {}), "missing catalog must reject");
        model_assets::Cache cache;
        const auto cold = cache.Prepare(plan, assets);
        const auto warm = cache.Prepare(plan, assets);
        Check(cold->models_[0].model_ == warm->models_[0].model_,
              "unchanged verified GLB was parsed again");
        auto damaged = assets;
        damaged[0].bytes_[0] ^= 1;
        Reject([&] { cache.Prepare(plan, damaged); });
        Reject([&] { cache.Prepare(plan, {}); });
        auto wrong_type = assets;
        wrong_type[0].record_.media_type_ = "image/png";
        Reject([&] { cache.Prepare(plan, wrong_type); });
        Check(cache.Prepare(plan, assets)->models_[0].model_ == cold->models_[0].model_,
              "failed load poisoned last successful model cache");
        Check(cache.Prepare({}, {})->models_.empty(), "unused model retained in catalog");
        Check(cache.Prepare(plan, assets)->models_[0].model_ != cold->models_[0].model_,
              "model cache retained removed assets");
        auto duplicate = plan;
        duplicate.instructions_.insert(duplicate.instructions_.begin(), plan.instructions_[0]);
        duplicate.instructions_[0].node_.id_ = 50;
        for (std::size_t i = 1; i < duplicate.instructions_.size(); ++i)
            for (auto& input : duplicate.instructions_[i].inputs_)
                if (input) ++*input;
        ++duplicate.output_;
        Check(model_assets::Prepare(duplicate, assets)->models_.size() == 1,
              "duplicate asset consumers share CPU geometry");
        Reject([&] { model_assets::Prepare(plan, {}); });
        auto corrupt = assets;
        corrupt[0].bytes_[0] ^= 1;
        Reject([&] { model_assets::Prepare(plan, corrupt); });
        corrupt[0].record_.id_.sha256_ =
                picosha2::hash256_hex_string(corrupt[0].bytes_.begin(), corrupt[0].bytes_.end());
        auto malformed = plan;
        malformed.instructions_[0].node_.properties_["asset"] = corrupt[0].record_.id_;
        Reject([&] { model_assets::Prepare(malformed, corrupt); });
        corrupt = assets;
        corrupt.push_back(asset);
        Reject([&] { model_assets::Prepare(plan, corrupt); });
        std::stop_source cancellation;
        cancellation.request_stop();
        Reject([&] { model_assets::Prepare(plan, assets, cancellation.get_token()); });
        const auto package =
                project::DecodePackage(project::EncodePackage(document, "Box", assets));
        Check(model_assets::Prepare(package.program_, package.assets_)->models_[0].indices_ == 36,
              "model references and embedded bytes survive package roundtrip");
        Reject([&] { project::EncodePackage(document, "missing"); });
        const std::filesystem::path directory(argv[2]);
        assets::Store store(directory);
        const auto record = store.Import(argv[1], "model/gltf-binary");
        prepared_assets::Loader loader;
        loader.Submit({plan, {record}, directory, 1});
        loader.Submit({plan, {record}, directory, 2});
        loader.Submit({plan, {record}, directory, 3});
        std::optional<prepared_assets::LoadResult> loaded;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (!loaded && std::chrono::steady_clock::now() < deadline) {
            loaded = loader.Take();
            if (!loaded) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        Check(loaded && loaded->generation_ == 3 && loaded->error_.empty() && loaded->resources_,
              "latest generation alone commits prepared resources");
        loader.Submit({plan, {record}, directory, 4});
        loader.Cancel();
        while (loader.Busy() && std::chrono::steady_clock::now() < deadline) {
            Check(!loader.Take(), "cancelled completion must be suppressed");
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        Check(!loader.Busy(), "cancelled loader drains");
        std::cout << "Model resource preparation passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
