#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>

#include "rhythm/content/presets.h"
#include "rhythm/editor/history.h"

namespace {
void Check(bool value) {
    if (!value) throw std::runtime_error("content.contract");
}
}  // namespace
int main(int argc, char* argv[]) {
    using namespace rhythm;
    try {
        if (argc != 2) throw std::invalid_argument("test.arguments");
        graph::Registry registry;
        const auto presets = content::LoadPresets(argv[1], registry);
        Check(presets.size() == 106);
        for (const auto& descriptor : registry.Operators()) {
            if (descriptor.properties_.empty()) continue;
            bool has_default = false;
            for (const auto& preset : presets)
                if (preset.operator_type_ == descriptor.type_ && preset.id_.ends_with(".default")) {
                    has_default = true;
                    const auto node = registry.MakeNode(1, descriptor.type_);
                    Check(content::ApplyPreset(node, preset, registry) == node);
                }
            Check(has_default);
        }
        editor::Snapshot original;
        original.document_.id_ = "content.test";
        original.document_.nodes_ = {registry.MakeNode(17, "texture.gradient")};
        editor::History history(original);
        auto next = history.Current();
        next.document_.nodes_[0] =
                content::ApplyPreset(next.document_.nodes_[0], presets[1], registry);
        Check(next.document_.nodes_[0].id_ == 17 &&
              next.document_.nodes_[0] != original.document_.nodes_[0]);
        Check(history.Apply(next, 0) && history.Undo());
        Check(history.Current().document_.nodes_ == original.document_.nodes_);
        Check(history.Redo() && history.Current().document_.nodes_ == next.document_.nodes_);
        auto bad_preset = presets[1];
        bad_preset.properties_["unknown"] = 1.0;
        bool rejected = false;
        try {
            content::ApplyPreset(original.document_.nodes_[0], bad_preset, registry);
        } catch (const std::exception&) {
            rejected = true;
        }
        Check(rejected);
        std::ifstream file(argv[1]);
        const auto catalog = nlohmann::json::parse(file);
        for (int mutation = 0; mutation < 4; ++mutation) {
            auto bad = catalog;
            if (mutation == 0) bad["presets"].push_back(bad["presets"][0]);
            if (mutation == 1) bad["presets"][0]["properties"]["color_a"] = {1, 0, 0, 99};
            if (mutation == 2) bad["presets"][0]["titles"].erase("zh-CN");
            if (mutation == 3) bad["presets"][0]["operator"] = "unknown";
            rejected = false;
            try {
                content::DecodePresets(bad.dump(), registry);
            } catch (const std::exception&) {
                rejected = true;
            }
            Check(rejected);
        }
        std::cout << "content contracts passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
