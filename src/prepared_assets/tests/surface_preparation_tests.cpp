#include <picosha2.h>

#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

#include "preparation_cache.h"
#include "rhythm/assets/store.h"
#include "rhythm/prepared_assets/loader.h"

namespace {
void Require(bool value) {
    if (!value) throw std::runtime_error("surface.preparation_contract");
}
template <typename Function>
void Reject(Function function) {
    try {
        function();
    } catch (const std::exception&) {
        return;
    }
    throw std::runtime_error("surface.invalid_preparation_accepted");
}
std::vector<std::uint8_t> Read(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    const auto size = file.tellg();
    if (!file || size < 24 || size > 512 * 1024) throw std::runtime_error("surface.fixture");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(bytes.data()), size);
    if (!file) throw std::runtime_error("surface.read");
    return bytes;
}
}  // namespace
int main(int argc, char* argv[]) {
    using namespace rhythm;
    try {
        if (argc != 2) throw std::runtime_error("surface.fixture_directory_required");
        const std::filesystem::path directory(argv[1]);
        surface_shader::Program program;
        program.expression_ = "clamp(vec3(a, b, c) * d, vec3(0.0, 0.0, 0.0), vec3(1.0, 1.0, 1.0))";
        program.compiler_sha256_ = std::string(64, 'a');
        program.artifacts_ = {Read(directory / "windows/parameter_surface.bin"),
                              Read(directory / "android/parameter_surface.bin")};
        const auto encoded = surface_shader::Encode(program);
        const std::string bytes(encoded.begin(), encoded.end());
        project::PackagedAsset asset{{{picosha2::hash256_hex_string(bytes.begin(), bytes.end())},
                                      bytes.size(),
                                      std::string(surface_shader::kMediaType)},
                                     bytes};
        graph::Registry registry;
        graph::Document document;
        document.id_ = "surface.preparation";
        document.nodes_ = {
                registry.MakeNode(1, "material.unlit"),  registry.MakeNode(2, "material.shader"),
                registry.MakeNode(3, "geometry.sphere"), registry.MakeNode(4, "scene.instance"),
                registry.MakeNode(5, "scene.render"),    registry.MakeNode(6, "output.texture")};
        document.nodes_[1].properties_["asset"] = asset.record_.id_;
        document.edges_ = {{1, 1, 2, "material"},
                           {2, 3, 4, "geometry"},
                           {3, 2, 4, "material"},
                           {4, 4, 5, "scene"},
                           {5, 5, 6, "source"}};
        document.output_ = 6;
        const auto plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
        std::vector<project::PackagedAsset> assets{asset};
        prepared_assets::detail::PreparationCache cache;
        Require(!prepared_assets::Covers(plan, prepared_assets::Resources{}));
        const auto first = cache.Prepare(plan, assets);
        const auto warm = cache.Prepare(plan, assets);
        Require(prepared_assets::Covers(plan, *first) && prepared_assets::Covers(plan, *warm) &&
                warm->surfaces_->programs_.size() == 1 && warm->shaders_->programs_.empty());
        assets[0].bytes_[0] ^= 1;
        Reject([&] { cache.Prepare(plan, assets); });
        assets[0] = asset;
        assets[0].record_.media_type_ = std::string(image_shader::kMediaType);
        Reject([&] { cache.Prepare(plan, assets); });
        Reject([&] { cache.Prepare(plan, {}); });
        assets[0] = asset;
        std::stop_source cancel;
        cancel.request_stop();
        Reject([&] { cache.Prepare(plan, assets, cancel.get_token()); });
        Require(prepared_assets::Covers(plan, *cache.Prepare(plan, assets)));
        const auto bundle = directory / "prepared-surface.rmsurface";
        {
            std::ofstream file(bundle, std::ios::binary);
            file.write(bytes.data(), std::streamsize(bytes.size()));
            if (!file) throw std::runtime_error("surface.fixture_write");
        }
        assets::Store store(directory / "prepared-surface-assets");
        const auto record = store.Import(bundle, std::string(surface_shader::kMediaType));
        prepared_assets::Loader loader;
        loader.Submit({plan, {record}, directory / "prepared-surface-assets", 17});
        std::optional<prepared_assets::LoadResult> loaded;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (!loaded && std::chrono::steady_clock::now() < deadline) {
            loaded = loader.Take();
            if (!loaded) std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        Require(loaded && loaded->error_.empty() && loaded->generation_ == 17 &&
                loaded->resources_ && prepared_assets::Covers(plan, *loaded->resources_));
        std::cout << "Surface preparation: coverage, warm hash/MIME rejection, cancellation "
                     "recovery and asynchronous asset loading passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
