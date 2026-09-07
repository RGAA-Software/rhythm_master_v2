#include "rhythm/graph/registry.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "point_descriptors.h"
#include "rhythm/graph/components.h"
#include "scene_descriptors.h"

namespace rhythm::graph {
Registry::Registry() {
    using Type = ValueType;
    operators_ = {
            {"core.time", Operation::kTime, Type::kScalar, {}, {}, true},
            {"texture.image",
             Operation::kTextureImage,
             Type::kTexture,
             {},
             {{"asset", assets::AssetId{}},
              {"image_fill", 0.0, 0, 1, {"image.fit", "image.fill"}, true}}},
            {"texture.video",
             Operation::kTextureVideo,
             Type::kTexture,
             {},
             {{"asset", assets::AssetId{}},
              {"video_speed", 1.0, 0, 4},
              {"video_offset", 0.0, 0, 3600},
              {"video_loop", 1.0, 0, 1, {"video.hold", "video.repeat"}, true},
              {"image_fill", 0.0, 0, 1, {"image.fit", "image.fill"}, true}}},
            {"signal.oscillator",
             Operation::kOscillator,
             Type::kSignal,
             {{"time", Type::kScalar}},
             {{"frequency", 0.25, 0.01, 20}},
             true},
            {"signal.sample", Operation::kSample, Type::kScalar, {{"signal", Type::kSignal}}},
            {"texture.gradient",
             Operation::kGradient,
             Type::kTexture,
             {{"amount", Type::kScalar, false}},
             {{"color_a", Color{0.02, 0.6, 0.85, 1}}, {"color_b", Color{0.6, 0.02, 0.4, 1}}}},
            {"texture.transform",
             Operation::kTransform,
             Type::kTexture,
             {{"source", Type::kTexture}},
             {{"scale", 0.95, 0.1, 2}}},
            {"texture.blend",
             Operation::kBlend,
             Type::kTexture,
             {{"a", Type::kTexture}, {"b", Type::kTexture}},
             {{"amount", 0.25, 0, 1}}},
            {"texture.feedback",
             Operation::kFeedback,
             Type::kTexture,
             {{"source", Type::kTexture}}},
            {"output.texture", Operation::kOutput, Type::kTexture, {{"source", Type::kTexture}}},
            {"scalar.curve",
             Operation::kCurve,
             Type::kScalar,
             {{"time", Type::kScalar}},
             {{"curve", parameters::Curve{}}}},
            {"scalar.constant",
             Operation::kConstant,
             Type::kScalar,
             {},
             {{"value", 1.0, -1000000, 1000000}}},
            {"scalar.math",
             Operation::kMath,
             Type::kScalar,
             {{"a", Type::kScalar, false}, {"b", Type::kScalar, false}},
             {{"a", 0.0, -1000000, 1000000},
              {"b", 1.0, -1000000, 1000000},
              {"math_mode",
               0.0,
               0,
               5,
               {"math.add", "math.subtract", "math.multiply", "math.divide", "math.minimum",
                "math.maximum"}}}},
            {"time.local",
             Operation::kLocalTime,
             Type::kScalar,
             {{"time", Type::kScalar}},
             {{"speed", 1.0, -16, 16},
              {"offset", 0.0, -86400, 86400},
              {"duration", 4.0, 0.001, 86400},
              {"time_mode", 0.0, 0, 2, {"time.free", "time.loop", "time.ping_pong"}}}},
            {"scalar.map",
             Operation::kMap,
             Type::kScalar,
             {{"value", Type::kScalar}},
             {{"input_min", 0.0, -1000000, 1000000},
              {"input_max", 1.0, -1000000, 1000000},
              {"output_min", 0.0, -1000000, 1000000},
              {"output_max", 1.0, -1000000, 1000000},
              {"map_mode", 0.0, 0, 1, {"map.clamp", "map.extend"}}}},
            {"scalar.compare",
             Operation::kCompare,
             Type::kScalar,
             {{"a", Type::kScalar, false}, {"b", Type::kScalar, false}},
             {{"a", 0.0, -1000000, 1000000},
              {"b", 0.5, -1000000, 1000000},
              {"epsilon", 0.000001, 0, 1},
              {"compare_mode",
               0.0,
               0,
               5,
               {"compare.greater", "compare.less", "compare.equal", "compare.not_equal",
                "compare.greater_equal", "compare.less_equal"}}}},
            {"scalar.select",
             Operation::kSelect,
             Type::kScalar,
             {{"condition", Type::kScalar},
              {"a", Type::kScalar, false},
              {"b", Type::kScalar, false}},
             {{"a", 0.0, -1000000, 1000000}, {"b", 1.0, -1000000, 1000000}}},
            {"signal.noise",
             Operation::kNoise,
             Type::kSignal,
             {{"time", Type::kScalar}},
             {{"frequency", 1.0, 0.01, 100},
              {"seed", 0.0, 0, 4294967295.0, {}, true},
              {"noise_mode", 2.0, 0, 2, {"noise.step", "noise.linear", "noise.smooth"}}}},
            {"session.time",
             Operation::kSessionTime,
             Type::kScalar,
             {},
             {{"session_value", 0.0, 0, 1, {"session.seconds", "session.synchronized"}}}},
            {"participant.role",
             Operation::kParticipantRole,
             Type::kScalar,
             {},
             {{"role_value", 0.0, 0, 1, {"role.index", "role.group"}}}},
            {"participant.control",
             Operation::kSharedControl,
             Type::kScalar,
             {},
             {{"channel", 0.0, 0, 31, {}, true}}},
            {"audio.feature",
             Operation::kAudioFeature,
             Type::kScalar,
             {},
             {{"audio_feature",
               1.0,
               0,
               6,
               {"audio.rms", "audio.loudness", "audio.onset", "audio.bpm", "audio.confidence",
                "audio.centroid", "audio.available"}}}},
            {"audio.band",
             Operation::kAudioBand,
             Type::kScalar,
             {},
             {{"audio_band", 20.0, 0, 62, {}, true},
              {"audio_channel", 0.0, 0, 2, {"audio.mono", "audio.left", "audio.right"}}}},
            {"texture.color_adjust",
             Operation::kColorAdjust,
             Type::kTexture,
             {{"source", Type::kTexture},
              {"exposure", Type::kScalar, false},
              {"contrast", Type::kScalar, false},
              {"saturation", Type::kScalar, false},
              {"invert", Type::kScalar, false}},
             {{"exposure", 0.0, -8, 8},
              {"contrast", 1.0, 0, 4},
              {"saturation", 1.0, 0, 4},
              {"invert", 0.0, 0, 1}}},
            {"scalar.expression",
             Operation::kExpression,
             Type::kScalar,
             {{"a", Type::kScalar, false},
              {"b", Type::kScalar, false},
              {"c", Type::kScalar, false},
              {"time", Type::kScalar, false}},
             {{"expression", parameters::Expression{}},
              {"a", 0.0, -1000000, 1000000},
              {"b", 0.0, -1000000, 1000000},
              {"c", 0.0, -1000000, 1000000},
              {"time", 0.0, 0, 86400}}},
            {"texture.shape",
             Operation::kShape,
             Type::kTexture,
             {},
             {{"shape_type",
               1.0,
               0,
               3,
               {"shape.rectangle", "shape.ellipse", "shape.ring", "shape.polygon"}},
              {"shape_width", 0.8, 0, 2},
              {"shape_height", 0.8, 0, 2},
              {"center_x", 0.5, 0, 1},
              {"center_y", 0.5, 0, 1},
              {"inner_ratio", 0.75, 0, 1},
              {"sides", 6.0, 3, 128, {}, true},
              {"color_a", Color{1, 1, 1, 1}}}},
            {"texture.mask",
             Operation::kMask,
             Type::kTexture,
             {{"source", Type::kTexture}, {"mask", Type::kTexture}},
             {{"mask_mode", 0.0, 0, 1, {"mask.alpha", "mask.inverse_alpha"}}}},
            {"texture.composite",
             Operation::kComposite,
             Type::kTexture,
             {{"a", Type::kTexture}, {"b", Type::kTexture}, {"amount", Type::kScalar, false}},
             {{"amount", 1.0, 0, 1},
              {"composite_mode", 0.0, 0, 1, {"composite.over", "composite.add"}}}},
            {"texture.affine",
             Operation::kAffine,
             Type::kTexture,
             {{"source", Type::kTexture},
              {"scale", Type::kScalar, false},
              {"rotation", Type::kScalar, false},
              {"translate_x", Type::kScalar, false},
              {"translate_y", Type::kScalar, false},
              {"opacity", Type::kScalar, false}},
             {{"scale", 1.0, 0, 8},
              {"scale_x", 1.0, -8, 8},
              {"scale_y", 1.0, -8, 8},
              {"rotation", 0.0, -36000, 36000},
              {"translate_x", 0.0, -4, 4},
              {"translate_y", 0.0, -4, 4},
              {"pivot_x", 0.5, 0, 1},
              {"pivot_y", 0.5, 0, 1},
              {"opacity", 1.0, 0, 1}}},
            {"texture.spectrum",
             Operation::kAudioSpectrum,
             Type::kTexture,
             {},
             {{"bar_count", 63.0, 8, 256, {}, true},
              {"spectrum_gain", 2.0, 0, 16},
              {"bar_gap", 0.2, 0, 0.9},
              {"spectrum_radius", 0.2, 0.05, 0.4},
              {"spectrum_layout", 0.0, 0, 1, {"spectrum.linear", "spectrum.radial"}},
              {"audio_channel", 0.0, 0, 2, {"audio.mono", "audio.left", "audio.right"}},
              {"color_a", Color{0.05, 0.9, 0.8, 1}},
              {"color_b", Color{0.2, 0.1, 0.8, 1}}}}};
    AppendPointDescriptors(operators_);
    operators_.push_back({"texture.stack",
                          Operation::kTextureStack,
                          Type::kTexture,
                          {{"layer_1", Type::kTexture},
                           {"layer_2", Type::kTexture, false},
                           {"layer_3", Type::kTexture, false},
                           {"layer_4", Type::kTexture, false},
                           {"layer_5", Type::kTexture, false},
                           {"layer_6", Type::kTexture, false},
                           {"layer_7", Type::kTexture, false},
                           {"layer_8", Type::kTexture, false}},
                          {{"composite_mode", 0.0, 0, 1, {"composite.over", "composite.add"}}}});
    AppendSceneDescriptors(operators_);
    operators_.push_back({"texture.noise",
                          Operation::kTextureNoise,
                          Type::kTexture,
                          {{"phase", Type::kScalar, false}},
                          {{"noise_scale", 4.0, 0.25, 32},
                           {"phase", 0.0, 0, 4096},
                           {"contrast", 1.0, 0, 4},
                           {"seed", 0.0, 0, 1024},
                           {"color_a", Color{0.015, 0.03, 0.12, 1}},
                           {"color_b", Color{0.12, 0.65, 0.8, 1}}}});
    operators_.push_back({"texture.blur",
                          Operation::kGaussianBlur,
                          Type::kTexture,
                          {{"source", Type::kTexture}, {"blur_radius", Type::kScalar, false}},
                          {{"blur_radius", 6.0, 0, 32}}});
    operators_.push_back({"texture.mapping",
                          Operation::kTextureMapping,
                          Type::kTexture,
                          {{"source", Type::kTexture},
                           {"rotation", Type::kScalar, false},
                           {"travel", Type::kScalar, false},
                           {"twist", Type::kScalar, false},
                           {"scale", Type::kScalar, false}},
                          {{"mapping_mode", 0.0, 0, 1, {"mapping.kaleidoscope", "mapping.polar"}},
                           {"scale", 1.0, 0.125, 16},
                           {"rotation", 0.0, -36000, 36000},
                           {"travel", 0.0, -4096, 4096},
                           {"twist", 0.0, -16, 16},
                           {"sectors", 8.0, 1, 32, {}, true},
                           {"radial_power", 1.0, -2, 4}}});
    operators_.push_back({"texture.contours",
                          Operation::kTextureContours,
                          Type::kTexture,
                          {{"source", Type::kTexture}, {"phase", Type::kScalar, false}},
                          {{"contour_count", 12.0, 1, 64},
                           {"line_width", 0.12, 0.01, 0.49},
                           {"phase", 0.0, -4096, 4096},
                           {"color_a", Color{0.02, 0.6, 1, 1}},
                           {"color_b", Color{1, 0.12, 0.35, 1}}}});
    operators_.push_back(
            {"texture.displace",
             Operation::kTextureDisplace,
             Type::kTexture,
             {{"source", Type::kTexture},
              {"displace_map", Type::kTexture},
              {"displace_strength", Type::kScalar, false},
              {"rotation", Type::kScalar, false}},
             {{"displace_mode", 0.0, 0, 1, {"displace.gradient", "displace.vector_rg"}},
              {"displace_strength", 0.05, -1, 1},
              {"sample_radius", 2.0, 1, 32},
              {"rotation", 0.0, -36000, 36000}}});
    operators_.push_back({"texture.trail",
                          Operation::kTextureTrail,
                          Type::kTexture,
                          {{"source", Type::kTexture}, {"trail_half_life", Type::kScalar, false}},
                          {{"trail_half_life", 0.5, 0, 5},
                           {"trail_zoom_rate", 0.0, -0.5, 0.5},
                           {"trail_rotation_rate", 0.0, -180, 180}},
                          true});
}
std::optional<OperatorDescriptor> Registry::Find(
        std::string_view type, std::span<const ComponentDefinition> components) const {
    const auto found = std::find_if(operators_.begin(), operators_.end(),
                                    [&](const auto& item) { return item.type_ == type; });
    if (found == operators_.end())
        return components.empty() ? std::nullopt : DescribeComponent(type, *this, components);
    return *found;
}
Node Registry::MakeNode(NodeId id, std::string_view type,
                        std::span<const ComponentDefinition> components) const {
    const auto descriptor = Find(type, components);
    if (!descriptor) throw std::invalid_argument("graph.unknown_operator");
    Node node;
    node.id_ = id;
    node.type_ = type;
    for (const auto& property : descriptor->properties_)
        node.properties_[property.key_] = property.default_;
    return node;
}
std::vector<Diagnostic> Registry::ValidateNode(
        const Node& node, std::span<const ComponentDefinition> components) const {
    std::vector<Diagnostic> diagnostics;
    const auto descriptor = Find(node.type_, components);
    if (!descriptor || node.version_ != 1) return {{"graph.unsupported_node", node.id_}};
    for (const auto& [key, value] : node.properties_) {
        const auto expected =
                std::find_if(descriptor->properties_.begin(), descriptor->properties_.end(),
                             [&](const auto& item) { return item.key_ == key; });
        if (expected == descriptor->properties_.end() ||
            expected->default_.index() != value.index()) {
            diagnostics.push_back({"graph.property_type", node.id_, key});
            continue;
        }
        bool valid = true;
        if (std::holds_alternative<double>(value)) {
            const auto number = std::get<double>(value);
            valid = std::isfinite(number) && number >= expected->minimum_ &&
                    number <= expected->maximum_;
            if (!expected->choices_.empty() || expected->integral_)
                valid = valid && std::floor(number) == number;
        } else if (std::holds_alternative<Color>(value)) {
            const auto color = std::get<Color>(value);
            for (const auto channel : {color.r_, color.g_, color.b_, color.a_})
                valid = valid && std::isfinite(channel) && channel >= 0 && channel <= 1;
        }
        if (std::holds_alternative<parameters::Curve>(value))
            valid = !std::get<parameters::Curve>(value).Keys().empty();
        if (std::holds_alternative<assets::AssetId>(value)) {
            const auto& asset = std::get<assets::AssetId>(value);
            valid = asset.sha256_.empty() || assets::ValidId(asset);
        }
        if (!valid) diagnostics.push_back({"graph.property_range", node.id_, key});
    }
    return diagnostics;
}
double Scalar(const Node& node, std::string_view key, double fallback) {
    const auto found = node.properties_.find(std::string(key));
    if (found == node.properties_.end()) return fallback;
    return std::get<double>(found->second);
}
Color ColorValue(const Node& node, std::string_view key, Color fallback) {
    const auto found = node.properties_.find(std::string(key));
    if (found == node.properties_.end()) return fallback;
    return std::get<Color>(found->second);
}
}  // namespace rhythm::graph
