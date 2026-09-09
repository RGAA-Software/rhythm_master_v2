#include "operator_help.h"

#include <imgui.h>

#include <array>

namespace rhythm::studio {
void DrawOperatorHelp(const graph::OperatorDescriptor& descriptor,
                      const std::map<std::string, std::string>& text, bool properties) {
    const auto label = [&](const std::string& key) {
        const auto found = text.find(key);
        return found == text.end() ? key : found->second;
    };
    constexpr std::array kTypes{"scalar",   "signal", "texture", "points",     "geometry",
                                "material", "scene",  "camera",  "gpu_points", "scene_image",
                                "depth",    "path",   "event"};
    const auto type = [&](graph::ValueType value) {
        return label("help.type." + std::string(kTypes.at(static_cast<std::size_t>(value))));
    };
    ImGui::TextDisabled("%s", descriptor.type_.c_str());
    const auto summary = text.find("help." + descriptor.type_);
    if (summary != text.end()) ImGui::TextWrapped("%s", summary->second.c_str());
    ImGui::TextWrapped("%s: %s", label("help.output").c_str(), type(descriptor.output_).c_str());
    ImGui::TextWrapped("%s",
                       label("help.route." +
                             std::string(kTypes.at(static_cast<std::size_t>(descriptor.output_))))
                               .c_str());
    if (descriptor.inputs_.empty())
        ImGui::TextWrapped("%s", label("help.no_inputs").c_str());
    else {
        ImGui::SeparatorText(label("help.inputs").c_str());
        for (const auto& port : descriptor.inputs_)
            ImGui::TextWrapped("%s: %s (%s)", label(port.key_).c_str(), type(port.type_).c_str(),
                               label(port.required_ ? "help.required" : "help.optional").c_str());
    }
    if (!properties || descriptor.properties_.empty()) return;
    ImGui::SeparatorText(label("help.properties").c_str());
    for (const auto& property : descriptor.properties_) {
        const auto title = label(property.key_);
        if (const auto scalar = std::get_if<double>(&property.default_)) {
            ImGui::TextWrapped("%s: %.4g [%g, %g]", title.c_str(), *scalar, property.minimum_,
                               property.maximum_);
            if (!property.choices_.empty()) {
                std::string choices;
                for (std::size_t index = 0; index < property.choices_.size(); ++index) {
                    if (!choices.empty()) choices += ", ";
                    choices += std::to_string(index) + "=" + label(property.choices_[index]);
                }
                ImGui::TextWrapped("%s", choices.c_str());
            }
        } else if (std::holds_alternative<graph::Color>(property.default_))
            ImGui::TextWrapped("%s: %s", title.c_str(), label("help.color").c_str());
        else if (std::holds_alternative<assets::AssetId>(property.default_))
            ImGui::TextWrapped("%s: %s", title.c_str(), label("help.asset").c_str());
        else if (std::holds_alternative<std::string>(property.default_))
            ImGui::TextWrapped("%s: %s", title.c_str(), label("text.edit_help").c_str());
        else if (std::holds_alternative<parameters::EventTrack>(property.default_))
            ImGui::TextWrapped("%s: %s", title.c_str(), label("event.track_help").c_str());
        else
            ImGui::TextWrapped("%s: %s", title.c_str(), label("help.editor").c_str());
    }
}
}  // namespace rhythm::studio
