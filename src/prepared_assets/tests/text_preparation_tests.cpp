#include <picosha2.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

#include "preparation_cache.h"
#include "rhythm/prepared_assets/prepare.h"
#include "rhythm/runtime/runtime.h"

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
        prepared_assets::detail::PreparationCache cache;
        const auto cached = cache.Prepare(package.program_, package.assets_, {});
        const auto glyphs = cache.Text().Rasterizations();
        Check(glyphs > 0 &&
                      cached->images_->images_[0].rgba_ == resources->images_->images_[0].rgba_,
              "cached preparation differs from cold load");
        const auto layouts = cache.Text().LayoutRenders();
        const auto repeated = cache.Prepare(package.program_, package.assets_);
        Check(cache.Text().LayoutRenders() == layouts &&
                      repeated->images_->images_[0].rgba_ == cached->images_->images_[0].rgba_,
              "unchanged text layout rendered again");
        auto wrong_mime = package.assets_;
        wrong_mime[0].record_.media_type_ = "image/png";
        Reject([&] { cache.Prepare(package.program_, wrong_mime); });
        auto warm_corrupt = package.assets_;
        warm_corrupt[0].bytes_[0] ^= 1;
        Reject([&] { cache.Prepare(package.program_, warm_corrupt); });
        std::stop_source stopped;
        stopped.request_stop();
        Reject([&] { cache.Prepare(package.program_, package.assets_, stopped.get_token()); });
        cache.Prepare(package.program_, package.assets_);
        Check(cache.Text().LayoutRenders() == layouts,
              "failed or cancelled preparation discarded the successful layout");
        auto rearranged = package.program_;
        rearranged.instructions_[0].node_.properties_["text_content"] =
                std::string("Rhythm / 棱镜星莲");
        const auto rearranged_resources = cache.Prepare(rearranged, package.assets_, {});
        Check(cache.Text().FontCount() == 1 && cache.Text().Rasterizations() == glyphs &&
                      cache.Text().LayoutRenders() == layouts + 1 &&
                      rearranged_resources->images_->images_[0].rgba_ !=
                              cached->images_->images_[0].rgba_,
              "rearranging existing glyphs rasterized them again or retained old layout");
        rearranged.instructions_[0].node_.properties_["text_size"] = 56.0;
        cache.Prepare(rearranged, package.assets_, {});
        Check(cache.Text().Rasterizations() > glyphs, "font size did not invalidate glyph cache");
        auto corrupt = package.assets_;
        corrupt[0].bytes_[0] ^= 1;
        Reject([&] { cache.Prepare(package.program_, corrupt, {}); });
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
            cache.Prepare(alternate, distinct, {});
            Check(cache.Text().FontCount() == 2, "font cache exceeded two retained fonts");
        }
        const auto before_reload = cache.Text().Rasterizations();
        cache.Prepare(package.program_, package.assets_, {});
        Check(cache.Text().FontCount() == 2 && cache.Text().Rasterizations() > before_reload,
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
