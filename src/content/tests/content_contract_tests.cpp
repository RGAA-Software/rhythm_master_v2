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
        std::ifstream file(argv[1]);
        const auto catalog = nlohmann::json::parse(file);
        const auto presets = content::LoadPresets(argv[1], registry);
        Check(presets.size() == catalog.at("presets").size());
        bool complete_defaults = true;
        for (const auto& descriptor : registry.Operators()) {
            if (descriptor.properties_.empty()) continue;
            bool has_default = false;
            for (const auto& preset : presets)
                if (preset.operator_type_ == descriptor.type_ && preset.id_.ends_with(".default")) {
                    has_default = true;
                    const auto node = registry.MakeNode(1, descriptor.type_);
                    Check(content::ApplyPreset(node, preset, registry) == node);
                }
            if (!has_default) {
                std::cerr << "Missing default preset: " << descriptor.type_ << '\n';
                complete_defaults = false;
            }
        }
        Check(complete_defaults);
        {
            auto events = catalog;
            auto entry = catalog.at("presets").at(0);
            entry["operator"] = "event.input";
            entry["properties"] = {
                    {"actions",
                     {{"event_track",
                       {{{"id", 1}, {"seconds", 0.5}, {"kind", "pulse"}, {"value", 1}},
                        {{"id", 2}, {"seconds", 1.0}, {"kind", "gate"}, {"value", 0}}}},
                      {"last_id", 2}}}};
            events["presets"] = nlohmann::json::array({entry});
            const auto decoded = content::DecodePresets(events.dump(), registry);
            const auto& track =
                    std::get<parameters::EventTrack>(decoded[0].properties_.at("actions"));
            Check(track.Events().size() == 2 && track.LastId() == 2 &&
                  track.Events()[1].kind_ == parameters::EventKind::kGate &&
                  track.Events()[0].seconds_ == 0.5);
            for (int mutation = 0; mutation < 2; ++mutation) {
                auto bad = events;
                auto& actions = bad["presets"][0]["properties"]["actions"]["event_track"];
                if (mutation == 0) actions[1]["id"] = 1;
                if (mutation == 1) actions[0]["kind"] = "unknown";
                bool rejected = false;
                try {
                    content::DecodePresets(bad.dump(), registry);
                } catch (const std::exception&) {
                    rejected = true;
                }
                Check(rejected);
            }
        }
        {
            auto hermite = catalog;
            auto entry = catalog.at("presets").at(0);
            entry["operator"] = "scalar.curve";
            entry["properties"] = {{"curve",
                                    {{"curve",
                                      {{{"seconds", 0},
                                        {"value", 0},
                                        {"interpolation", "hermite"},
                                        {"out_slope", 1}},
                                       {{"seconds", 2},
                                        {"value", 1},
                                        {"interpolation", "linear"},
                                        {"in_slope", 0}}}}}}};
            hermite["presets"] = nlohmann::json::array({entry});
            const auto decoded = content::DecodePresets(hermite.dump(), registry);
            const auto& curve = std::get<parameters::Curve>(decoded[0].properties_.at("curve"));
            Check(curve.Evaluate(0.5) == 0.4375 && curve.Keys()[0].out_slope_ == 1);
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
