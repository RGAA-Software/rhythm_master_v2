#include <picosha2.h>

#include <fstream>
#include <iostream>
#include <stdexcept>

#include "preparation_cache.h"

namespace {
void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
template <class Function>
void Reject(Function function) {
    bool rejected = false;
    try {
        function();
    } catch (const std::exception&) {
        rejected = true;
    }
    Check(rejected, "invalid warm preparation accepted");
}
rhythm::project::PackagedAsset Read(const std::filesystem::path& path, std::string mime) {
    std::ifstream stream(path, std::ios::binary);
    Check(bool(stream), "fixture unavailable");
    std::string bytes{std::istreambuf_iterator<char>(stream), {}};
    rhythm::assets::AssetRecord record{{picosha2::hash256_hex_string(bytes.begin(), bytes.end())},
                                       bytes.size(),
                                       std::move(mime)};
    return {std::move(record), std::move(bytes)};
}
}  // namespace

int main(int argc, char** argv) {
    using namespace rhythm;
    try {
        Check(argc == 5, "font, model, image and video fixtures required");
        const std::vector<project::PackagedAsset> assets{
                Read(argv[1], "font/otf"), Read(argv[2], "model/gltf-binary"),
                Read(argv[3], "image/png"), Read(argv[4], "video/x-matroska")};
        graph::Registry registry;
        graph::Document document;
        document.id_ = "mixed.preparation.cache";
        document.nodes_ = {
                registry.MakeNode(1, "texture.text"),  registry.MakeNode(2, "texture.text"),
                registry.MakeNode(3, "geometry.glb"),  registry.MakeNode(4, "scene.instance"),
                registry.MakeNode(5, "scene.render"),  registry.MakeNode(6, "texture.image"),
                registry.MakeNode(7, "texture.video"), registry.MakeNode(8, "texture.stack"),
                registry.MakeNode(9, "output.texture")};
        document.nodes_[0].properties_["asset"] = assets[0].record_.id_;
        document.nodes_[0].properties_["text_content"] = std::string("Rhythm");
        document.nodes_[1].properties_["asset"] = assets[0].record_.id_;
        document.nodes_[1].properties_["text_content"] = std::string("Master");
        document.nodes_[2].properties_["asset"] = assets[1].record_.id_;
        document.nodes_[5].properties_["asset"] = assets[2].record_.id_;
        document.nodes_[6].properties_["asset"] = assets[3].record_.id_;
        document.edges_ = {{1, 3, 4, "geometry"}, {2, 4, 5, "scene"},   {3, 1, 8, "layer_1"},
                           {4, 2, 8, "layer_2"},  {5, 5, 8, "layer_3"}, {6, 6, 8, "layer_4"},
                           {7, 7, 8, "layer_5"},  {8, 8, 9, "source"}};
        document.output_ = 9;
        auto plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
        prepared_assets::detail::PreparationCache cache;
        const auto first = cache.Prepare(plan, assets);
        Check(first->images_->images_.size() == 3 && first->videos_.size() == 1 &&
                      first->models_->models_.size() == 1 && cache.Text().LayoutRenders() == 2,
              "mixed fixture did not prepare all domains");
        for (auto& instruction : plan.instructions_)
            if (instruction.node_.id_ == 1)
                instruction.node_.properties_["text_content"] = std::string("Changed");
        const auto edited = cache.Prepare(plan, assets);
        Check(prepared_assets::Covers(plan, *edited), "edited resource coverage");
        Check(cache.Text().LayoutRenders() == 3, "text edit regenerated unchanged layout");
        Check(first->models_->models_[0].model_ == edited->models_->models_[0].model_,
              "text edit reparsed unrelated model");
        Check(first->videos_[0].bytes_ == edited->videos_[0].bytes_ &&
                      first->videos_[0].first_ == edited->videos_[0].first_,
              "text edit copied video source or decoded its first frame again");
        Check(first->images_->images_[0].rgba_ == edited->images_->images_[0].rgba_ &&
                      first->images_->images_[1].rgba_ != edited->images_->images_[1].rgba_ &&
                      first->images_->images_[2].rgba_ == edited->images_->images_[2].rgba_,
              "text edit damaged static image, retained old text or changed untouched text");
        for (std::size_t index = 0; index < assets.size(); ++index) {
            auto corrupt = assets;
            corrupt[index].bytes_[0] ^= 1;
            Reject([&] { cache.Prepare(plan, corrupt); });
            auto wrong_mime = assets;
            wrong_mime[index].record_.media_type_ = "application/octet-stream";
            Reject([&] { cache.Prepare(plan, wrong_mime); });
        }
        std::stop_source stop;
        stop.request_stop();
        Reject([&] { cache.Prepare(plan, assets, stop.get_token()); });
        const auto recovered = cache.Prepare(plan, assets);
        Check(cache.Text().LayoutRenders() == 3 &&
                      recovered->videos_[0].first_ == first->videos_[0].first_,
              "rejected preparation lost successful resource reuse");
        const auto empty = cache.Prepare({}, {});
        Check(empty->models_->models_.empty() && empty->images_->images_.empty() &&
                      empty->videos_.empty() && empty->shaders_->programs_.empty(),
              "removed resources retained in published catalog");
        const auto reloaded = cache.Prepare(plan, assets);
        Check(cache.Text().LayoutRenders() == 5 &&
                      reloaded->videos_[0].first_ != first->videos_[0].first_ &&
                      reloaded->models_->models_[0].model_ != first->models_->models_[0].model_,
              "cache retained removed prepared resources");
        std::cout << "Mixed asset edits preserve unrelated preparation and source validation\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
