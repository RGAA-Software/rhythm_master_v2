#include "bgfx_scene_morph.h"

#include <algorithm>
#include <stdexcept>

#include "scene_shader.h"

namespace rhythm::render::detail {
namespace {
struct LookupVertex {
    SkinWeights skin_{};
    float index_ = 0;
};
constexpr auto kFlags = BGFX_SAMPLER_POINT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
}  // namespace
BgfxSceneMorph::BgfxSceneMorph() {
    static_assert(sizeof(LookupVertex) == 24);
    const auto caps = bgfx::getCaps();
    // This pinned bgfx GLES backend never sets FORMAT_TEXTURE_VERTEX (unlike
    // D3D/Metal/Vulkan). Our GLES 3.1 profile has vertex samplers; require native
    // RGBA32F 2D support there and validate linking/drawing on the target device.
    const auto required = caps && caps->rendererType == bgfx::RendererType::OpenGLES
                                  ? BGFX_CAPS_FORMAT_TEXTURE_2D
                                  : BGFX_CAPS_FORMAT_TEXTURE_VERTEX;
    if (!caps || !(caps->formats[bgfx::TextureFormat::RGBA32F] & required))
        throw std::runtime_error("render.morph_format");
    layout_.begin()
            .add(bgfx::Attrib::Indices, 4, bgfx::AttribType::Uint8, true)
            .add(bgfx::Attrib::Weight, 4, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord1, 1, bgfx::AttribType::Float)
            .end();
    GpuHandle vertex(bgfx::createShader(
            bgfx::copy(kSceneMorphVertexShader, sizeof(kSceneMorphVertexShader))));
    GpuHandle instance(bgfx::createShader(
            bgfx::copy(kSceneMorphInstanceShader, sizeof(kSceneMorphInstanceShader))));
    GpuHandle fragment(
            bgfx::createShader(bgfx::copy(kSceneFragmentShader, sizeof(kSceneFragmentShader))));
    ordinary_ = GpuHandle(bgfx::createProgram(vertex.Get(), fragment.Get(), false));
    instances_ = GpuHandle(bgfx::createProgram(instance.Get(), fragment.Get(), false));
    bones_ = GpuHandle(bgfx::createUniform("u_skin_bones", bgfx::UniformType::Mat4,
                                           static_cast<std::uint16_t>(kMaximumSkinBones)));
    weights_ = GpuHandle(bgfx::createUniform("u_morph_weights", bgfx::UniformType::Vec4));
    info_ = GpuHandle(bgfx::createUniform("u_morph_info", bgfx::UniformType::Vec4));
    sampler_ = GpuHandle(bgfx::createUniform("s_morph_targets", bgfx::UniformType::Sampler));
    if (!bgfx::isValid(ordinary_.Get()) || !bgfx::isValid(instances_.Get()) ||
        !bgfx::isValid(bones_.Get()) || !bgfx::isValid(weights_.Get()) ||
        !bgfx::isValid(info_.Get()) || !bgfx::isValid(sampler_.Get()))
        throw std::runtime_error("render.morph_program");
}
MorphGeometry BgfxSceneMorph::Create(std::size_t vertices, std::span<const SkinWeights> skin,
                                     std::span<const MorphTarget> targets) const {
    MorphGeometry result;
    result.vertices_ = static_cast<std::uint32_t>(vertices);
    result.height_ = static_cast<std::uint16_t>((vertices * targets.size() * 3 + 1023) / 1024);
    result.targets_ = static_cast<std::uint8_t>(targets.size());
    const auto caps = bgfx::getCaps();
    if (!caps || caps->limits.maxTextureSize < 1024 ||
        result.height_ > caps->limits.maxTextureSize ||
        !bgfx::isTextureValid(0, false, 1, bgfx::TextureFormat::RGBA32F, kFlags))
        throw std::runtime_error("render.morph_format");
    std::vector<float> pixels(std::size_t(result.height_) * 1024 * 4);
    for (std::size_t target = 0; target < targets.size(); ++target)
        for (std::size_t vertex = 0; vertex < vertices; ++vertex) {
            const auto& delta = targets[target].deltas_[vertex];
            auto offset = (target * vertices + vertex) * 12;
            for (const auto& values : {delta.position_, delta.normal_, delta.tangent_}) {
                std::copy(values.begin(), values.end(),
                          pixels.begin() + static_cast<std::ptrdiff_t>(offset));
                offset += 4;
            }
        }
    std::vector<LookupVertex> lookup(vertices);
    for (std::size_t i = 0; i < vertices; ++i) {
        lookup[i].index_ = static_cast<float>(i);
        if (!skin.empty()) lookup[i].skin_ = skin[i];
    }
    result.lookup_ = GpuHandle(bgfx::createVertexBuffer(
            bgfx::copy(lookup.data(),
                       static_cast<std::uint32_t>(lookup.size() * sizeof(LookupVertex))),
            layout_));
    result.deltas_ = GpuHandle(bgfx::createTexture2D(
            1024, result.height_, false, 1, bgfx::TextureFormat::RGBA32F, kFlags,
            bgfx::copy(pixels.data(), static_cast<std::uint32_t>(pixels.size() * sizeof(float)))));
    if (!bgfx::isValid(result.lookup_.Get()) || !bgfx::isValid(result.deltas_.Get()))
        throw std::runtime_error("render.morph_buffer");
    return result;
}
void BgfxSceneMorph::Bind(const MorphGeometry& geometry, const MeshDraw& draw) const {
    bgfx::setVertexBuffer(1, geometry.lookup_.Get());
    bgfx::setTexture(6, sampler_.Get(), geometry.deltas_.Get(), kFlags);
    bgfx::setUniform(weights_.Get(), draw.morph_weights_.data());
    const std::array<float, 4> info{float(geometry.height_), float(geometry.vertices_),
                                    float(geometry.targets_), draw.bones_.empty() ? 0.0f : 1.0f};
    bgfx::setUniform(info_.Get(), info.data());
    if (!draw.bones_.empty())
        bgfx::setUniform(bones_.Get(), draw.bones_.data(),
                         static_cast<std::uint16_t>(draw.bones_.size()));
}
}  // namespace rhythm::render::detail
