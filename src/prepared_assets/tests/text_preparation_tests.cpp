#include <picosha2.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

#include "rhythm/prepared_assets/prepare.h"
#include "rhythm/runtime/runtime.h"
#include "text_assets.h"

namespace {
void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
template <class Function>
void Reject(Function function) {
    bool rejected = false;
    try {
        function();
    } catch (const std::exception&) {
        rejected = true;
    }
    Check(rejected, "invalid text preparation accepted");
}
}  // namespace
int main(int argc, char** argv) {
    using namespace rhythm;
    try {
        Check(argc == 2, "font required");
        std::ifstream file(argv[1], std::ios::binary);
        Check(bool(file), "font unavailable");
        std::string bytes{std::istreambuf_iterator<char>(file), {}};
        const assets::AssetId font{picosha2::hash256_hex_string(bytes.begin(), bytes.end())};
        std::vector<project::PackagedAsset> assets{{{font, bytes.size(), "font/otf"}, bytes}};
        graph::Registry registry;
        graph::Document document;
        document.id_ = "prepared-text";
        document.canvas_ = {512, 256};
        document.nodes_ = {registry.MakeNode(1, "texture.text"),
                           registry.MakeNode(2, "output.texture")};
        document.nodes_[0].properties_["asset"] = font;
        document.nodes_[0].properties_["text_content"] = std::string("棱镜星莲 / Rhythm");
        document.edges_ = {{1, 1, 2, "source"}};
        document.output_ = 2;
        auto package = project::DecodePackage(project::EncodePackage(document, "Text", assets));
        auto resources = prepared_assets::Prepare(package.program_, package.assets_);
        prepared_assets::detail::TextCache cache;
        const auto cached = prepared_assets::detail::PrepareWithTextCache(
                package.program_, package.assets_, cache, {});
        const auto glyphs = cache.Rasterizations();
        Check(glyphs > 0 &&
                      cached->images_->images_[0].rgba_ == resources->images_->images_[0].rgba_,
              "cached preparation differs from cold load");
        auto rearranged = package.program_;
        rearranged.instructions_[0].node_.properties_["text_content"] =
                std::string("Rhythm / 棱镜星莲");
        const auto rearranged_resources = prepared_assets::detail::PrepareWithTextCache(
                rearranged, package.assets_, cache, {});
        Check(cache.FontCount() == 1 && cache.Rasterizations() == glyphs &&
                      rearranged_resources->images_->images_[0].rgba_ !=
                              cached->images_->images_[0].rgba_,
              "rearranging existing glyphs rasterized them again or retained old layout");
        rearranged.instructions_[0].node_.properties_["text_size"] = 56.0;
        prepared_assets::detail::PrepareWithTextCache(rearranged, package.assets_, cache, {});
        Check(cache.Rasterizations() > glyphs, "font size did not invalidate glyph cache");
        auto corrupt = package.assets_;
        corrupt[0].bytes_[0] ^= 1;
        Reject([&] {
            prepared_assets::detail::PrepareWithTextCache(package.program_, corrupt, cache, {});
        });
        // Trailing padding is legal in these test font files. Distinct verified
        // identities exercise eviction without relying on a second system font.
        for (int identity = 1; identity <= 2; ++identity) {
            auto distinct = package.assets_;
            distinct[0].bytes_.append(identity, '\0');
            distinct[0].record_.bytes_ = distinct[0].bytes_.size();
            distinct[0].record_.id_.sha256_ = picosha2::hash256_hex_string(
                    distinct[0].bytes_.begin(), distinct[0].bytes_.end());
            auto alternate = package.program_;
            alternate.instructions_[0].node_.properties_["asset"] = distinct[0].record_.id_;
            prepared_assets::detail::PrepareWithTextCache(alternate, distinct, cache, {});
            Check(cache.FontCount() == 2, "font cache exceeded two retained fonts");
        }
        const auto before_reload = cache.Rasterizations();
        prepared_assets::detail::PrepareWithTextCache(package.program_, package.assets_, cache, {});
        Check(cache.FontCount() == 2 && cache.Rasterizations() > before_reload,
              "evicted font did not reload correctly");
        Check(prepared_assets::Covers(package.program_, *resources), "prepared text coverage");
        Check(resources->images_->images_.size() == 1 &&
                      !resources->images_->images_[0].missing_glyphs_,
              "CJK font coverage");
        auto renderer = render::Renderer::CreateNull();
        runtime::Runtime runtime;
        runtime::FrameContext frame;
        frame.extent_ = {512, 256};
        frame.images_ = resources->images_;
        renderer.BeginFrame();
        const auto first = runtime.Evaluate(package.program_, frame, renderer);
        Check(renderer.IsValid(first.final_) && !first.budget_, "text runtime failed");
        renderer.EndFrame();
        renderer.BeginFrame();
        frame.seconds_ = 1;
        Check(runtime.Evaluate(package.program_, frame, renderer).evaluated_ == 0,
              "static text rerendered");
        renderer.EndFrame();
        document.nodes_[0].properties_["text_content"] = std::string("音乐：Pulse，2026！");
        package = project::DecodePackage(project::EncodePackage(document, "Text", assets));
        Check(!prepared_assets::Covers(package.program_, *resources),
              "stale glyph layout accepted");
        renderer.BeginFrame();
        Reject([&] { runtime.Evaluate(package.program_, frame, renderer); });
        renderer.EndFrame();
        const auto changed = prepared_assets::Prepare(package.program_, package.assets_);
        Check(changed->images_->images_[0].rgba_ != resources->images_->images_[0].rgba_,
              "changed text retained old pixels");
        frame.images_ = changed->images_;
        renderer.BeginFrame();
        Check(renderer.IsValid(runtime.Evaluate(package.program_, frame, renderer).final_),
              "new text did not install");
        renderer.EndFrame();
        auto bad = package.assets_;
        bad[0].record_.media_type_ = "image/png";
        Reject([&] { prepared_assets::Prepare(package.program_, bad); });
        Reject([&] { prepared_assets::Prepare(package.program_, {}); });
        std::stop_source cancelled;
        cancelled.request_stop();
        Reject([&] {
            prepared_assets::Prepare(package.program_, package.assets_, cancelled.get_token());
        });
        std::cout << "Packaged font, prepared CJK masks, stale layout rejection and static runtime "
                     "reuse passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
