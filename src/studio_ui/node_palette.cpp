#include "node_palette.h"

#include <algorithm>
#include <array>

#include "operator_help.h"

namespace rhythm::studio {
namespace {
std::string_view Category(graph::Operation operation) {
    using enum graph::Operation;
    switch (operation) {
        case kConstant:
        case kControlScalar:
        case kMath:
        case kMap:
        case kCompare:
        case kSelect:
        case kExpression:
            return "palette.values";
        case kTime:
        case kLocalTime:
        case kTimeEnvelope:
        case kCurve:
        case kOscillator:
        case kNoise:
        case kSample:
            return "palette.time";
        case kAudioFeature:
        case kAudioBand:
        case kAudioSpectrum:
            return "palette.audio";
        case kEventBeat:
        case kEventCue:
        case kEventAudio:
        case kEventEdge:
        case kEventMerge:
        case kEventEnvelope:
        case kEventStep:
        case kEventGate:
        case kEventLatch:
        case kEventReset:
            return "palette.events";
        case kGradient:
        case kShape:
        case kTextureNoise:
        case kTextureShader:
        case kTextureImage:
        case kTextureVideo:
            return "palette.generators";
        case kTransform:
        case kAffine:
        case kColorAdjust:
        case kTextureLinearize:
        case kTextureDisplay:
        case kTextureFxaa:
        case kDepthLinearize:
        case kDepthOfField:
        case kGaussianBlur:
        case kTextureMapping:
        case kTextureDisplace:
        case kTextureContours:
            return "palette.filters";
        case kBlend:
        case kTextureStack:
        case kFeedback:
        case kTextureTrail:
        case kMask:
        case kComposite:
            return "palette.composite";
        case kPointGrid:
        case kParticleEmitter:
        case kGpuParticleEmitter:
        case kGpuPointRender:
        case kPointTransform:
        case kPointRender:
            return "palette.particles";
        case kPointPhysics:
            return "palette.physics";
        case kGeometryCube:
        case kGeometrySphere:
        case kGeometryTorus:
        case kGeometryGlb:
        case kSceneInstance:
        case kPointInstances:
        case kPathHelix:
        case kPathFromPoints:
        case kPathResample:
        case kGeometryTube:
        case kGeometryDeform:
        case kGeometryAnimate:
        case kGeometryMorph:
        case kSceneTransform:
        case kSceneMerge:
        case kSceneCamera:
        case kSceneRender:
        case kSceneCapture:
        case kSceneColor:
        case kSceneDepth:
            return "palette.scene";
        case kMaterialUnlit:
        case kMaterialPbr:
        case kMaterialTextures:
        case kDirectionalLight:
        case kPointLight:
        case kSpotLight:
        case kSceneEnvironment:
        case kSceneShadow:
            return "palette.materials";
        case kOutput:
            return "palette.output";
        case kComponent:
            return "palette.components";
        case kSessionTime:
        case kParticipantRole:
        case kSharedControl:
            return "palette.session";
    }
    return "palette.other";
}
constexpr std::array kCategories{"palette.generators", "palette.filters", "palette.composite",
                                 "palette.audio",      "palette.time",    "palette.values",
                                 "palette.particles",  "palette.physics", "palette.scene",
                                 "palette.materials",  "palette.output",  "palette.components",
                                 "palette.session",    "palette.events",  "palette.other"};
}  // namespace

std::optional<std::string> NodePalette::Draw(std::span<const graph::OperatorDescriptor> entries,
                                             const std::map<std::string, std::string>& text,
                                             const std::string& popup_id) {
    const auto label = [&](std::string_view key) {
        const auto found = text.find(std::string(key));
        return found == text.end() ? std::string(key) : found->second;
    };
    ImGui::PushID(popup_id.c_str());
    if (ImGui::Button((label("add_node") + "###add").c_str())) ImGui::OpenPopup(popup_id.c_str());
    // BeginPopup enables content autosizing; an explicit size each frame prevents
    // fill-available child regions and collapsed/filtered content from shrinking it.
    const auto available = ImGui::GetMainViewport()->WorkSize;
    ImGui::SetNextWindowSize({std::min(460.0f, std::max(1.0f, available.x - 16)),
                              std::min(540.0f, std::max(1.0f, available.y - 16))});
    std::optional<std::string> selected;
    if (ImGui::BeginPopup(popup_id.c_str())) {
        ImGui::TextUnformatted(label("palette.search").c_str());
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        filter_.Draw("###search", -1);
        ImGui::Separator();
        bool any_match = false;
        for (const auto category : kCategories) {
            const auto matches = [&](const graph::OperatorDescriptor& entry) {
                const auto searchable =
                        label(entry.type_) + " " + entry.type_ + " " + label(category);
                return Category(entry.operation_) == category &&
                       filter_.PassFilter(searchable.c_str());
            };
            std::size_t count = 0;
            for (const auto& entry : entries)
                if (matches(entry)) ++count;
            if (count == 0) continue;
            any_match = true;
            // Search reveals matches even in previously collapsed categories.
            if (filter_.IsActive()) ImGui::SetNextItemOpen(true, ImGuiCond_Always);
            const auto heading = label(category) + " (" + std::to_string(count) + ")###" + category;
            if (!ImGui::CollapsingHeader(heading.c_str())) continue;
            for (const auto& entry : entries) {
                if (!matches(entry)) continue;
                if (ImGui::Selectable((label(entry.type_) + "###" + entry.type_).c_str()))
                    selected = entry.type_;
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
                    ImGui::SetNextWindowSizeConstraints({320, 0}, {440, 500});
                    ImGui::BeginTooltip();
                    ImGui::PushTextWrapPos(400);
                    DrawOperatorHelp(entry, text, false);
                    ImGui::PopTextWrapPos();
                    ImGui::EndTooltip();
                }
            }
        }
        if (!any_match) ImGui::TextDisabled("%s", label("palette.empty").c_str());
        ImGui::EndPopup();
    }
    ImGui::PopID();
    return selected;
}
}  // namespace rhythm::studio
