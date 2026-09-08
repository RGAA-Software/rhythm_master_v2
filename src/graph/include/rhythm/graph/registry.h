#pragma once

#include <optional>
#include <span>

#include "rhythm/graph/document.h"

namespace rhythm::graph {
enum class Operation : std::uint8_t {
    kTime,
    kOscillator,
    kSample,
    kGradient,
    kTransform,
    kBlend,
    kFeedback,
    kOutput,
    kConstant,
    kMath,
    kLocalTime,
    kCurve,
    kMap,
    kCompare,
    kSelect,
    kNoise,
    kSessionTime,
    kParticipantRole,
    kSharedControl,
    kAudioFeature,
    kAudioBand,
    kAudioSpectrum,
    kAffine,
    kShape,
    kMask,
    kComposite,
    kExpression,
    kColorAdjust,
    kComponent,
    kPointGrid,
    kParticleEmitter,
    kPointTransform,
    kPointRender,
    kPointPhysics,
    kGeometryCube,
    kGeometrySphere,
    kMaterialUnlit,
    kSceneInstance,
    kSceneTransform,
    kSceneMerge,
    kSceneCamera,
    kSceneRender,
    kMaterialPbr,
    kDirectionalLight,
    kGeometryGlb,
    kGaussianBlur,
    kTextureNoise,
    kTextureMapping,
    kTextureContours,
    kTextureDisplace,
    kTextureTrail,
    kGeometryTorus,
    kTextureImage,
    kTextureVideo,
    kTextureStack,
    kTimeEnvelope,
    kPointInstances
};
struct PortDescriptor {
    std::string key_{};
    ValueType type_ = ValueType::kScalar;
    bool required_ = true;
};
struct PropertyDescriptor {
    std::string key_{};
    Property default_{0.0};
    double minimum_ = 0;
    double maximum_ = 1;
    std::vector<std::string> choices_{};
    bool integral_ = false;
    std::string group_{};
};
struct OperatorDescriptor {
    std::string type_{};
    Operation operation_ = Operation::kTime;
    ValueType output_ = ValueType::kScalar;
    std::vector<PortDescriptor> inputs_{};
    std::vector<PropertyDescriptor> properties_{};
    bool time_dependent_ = false;
};
class Registry final {
   public:
    Registry();
    std::optional<OperatorDescriptor> Find(
            std::string_view type, std::span<const ComponentDefinition> components = {}) const;
    std::span<const OperatorDescriptor> Operators() const { return operators_; }
    Node MakeNode(NodeId id, std::string_view type,
                  std::span<const ComponentDefinition> components = {}) const;
    std::vector<Diagnostic> ValidateNode(
            const Node& node, std::span<const ComponentDefinition> components = {}) const;

   private:
    std::vector<OperatorDescriptor> operators_{};
};
double Scalar(const Node& node, std::string_view key, double fallback);
Color ColorValue(const Node& node, std::string_view key, Color fallback);
}  // namespace rhythm::graph
