#include <iostream>
#include <stdexcept>

#include "rhythm/project/package.h"
#include "rhythm/project/store.h"
#include "rhythm/runtime/runtime.h"

int main(int argc, char* argv[]) {
    using namespace rhythm;
    try {
        if (argc != 2) throw std::invalid_argument("template.arguments");
        const auto templates = project::ScanTemplates(argv[1]);
        bool portrait = false;
        bool square = false;
        std::size_t defaults = 0;
        for (const auto& entry : templates) {
            defaults += entry.default_ ? 1 : 0;
            const auto source = project::LoadRevision(entry.directory_);
            if (!source.warnings_.empty()) throw std::runtime_error("template.layout");
            const auto& document = source.snapshot_.document_;
            portrait |= document.canvas_.height_ > document.canvas_.width_;
            square |= document.canvas_.height_ == document.canvas_.width_;
            const auto packaged = project::DecodePackage(
                    project::EncodePackage(document, source.snapshot_.title_));
            // Valid UTF-8 and available glyphs do not detect GBK-decoded mojibake.
            // Check the actual catalog label and published title, not a sample string.
            if (entry.id_ == "official.templates.prismatic_lotus") {
                const std::string expected = "棱镜星莲";
                if (entry.titles_.at("zh-CN") != expected ||
                    source.snapshot_.title_ != expected + " / Prismatic lotus" ||
                    packaged.title_ != source.snapshot_.title_)
                    throw std::runtime_error("template.localized_title");
            }
            if (packaged.program_.canvas_ != document.canvas_)
                throw std::runtime_error("template.canvas");
            auto renderer = render::Renderer::CreateNull();
            runtime::Runtime runtime;
            for (int frame = 0; frame < 60; ++frame) {
                renderer.BeginFrame();
                const auto output =
                        runtime.Evaluate(packaged.program_,
                                         {frame / 60.0,
                                          0,
                                          {static_cast<std::uint16_t>(document.canvas_.width_),
                                           static_cast<std::uint16_t>(document.canvas_.height_)}},
                                         renderer);
                if (!renderer.IsValid(output.final_)) throw std::runtime_error("template.output");
                renderer.EndFrame();
            }
            std::cout << entry.id_ << ": 60 runtime frames, " << document.canvas_.width_ << "x"
                      << document.canvas_.height_ << '\n';
        }
        if (templates.size() < 3 || defaults != 1 || !portrait || !square)
            throw std::runtime_error("template.catalog_coverage");
        std::cout << "template contracts passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
