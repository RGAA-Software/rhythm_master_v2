#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include <array>
#include <iostream>
#include <stdexcept>

#include "bgfx_backend.h"
#include "gpu_execution_probe.h"
#include "rhythm/player/render_quality.h"
#include "rhythm/player/session.h"
#include "rhythm/render/renderer.h"

namespace rhythm::validation {
void VerifyPointPixels(render::Renderer& renderer);
void VerifyEffectPixels(render::Renderer& renderer);
void VerifyVideoUploadPixels(render::Renderer& renderer);
void VerifyTextureReusePixels(render::Renderer& renderer);
void VerifyReadbackPixels(render::Renderer& renderer);
#ifdef RHYTHM_HAS_LOCAL_MEDIA
void VerifyMusicPackage(render::Renderer& renderer, const std::filesystem::path& path,
                        bool arrangement);
#endif
void MeasureTemplate(render::Renderer& renderer, const std::filesystem::path& path,
                     player::RenderQuality quality);
}  // namespace rhythm::validation

namespace rhythm::platform {
namespace {
class EglSurface final {
   public:
    EglSurface() = default;
    EglSurface(const EglSurface&) = delete;
    EglSurface& operator=(const EglSurface&) = delete;
    ~EglSurface() {
        if (display_ == EGL_NO_DISPLAY) return;
        eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (context_ != EGL_NO_CONTEXT) eglDestroyContext(display_, context_);
        if (surface_ != EGL_NO_SURFACE) eglDestroySurface(display_, surface_);
        eglTerminate(display_);
    }
    void Initialize() {
        display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (display_ == EGL_NO_DISPLAY || !eglInitialize(display_, nullptr, nullptr))
            throw std::runtime_error("probe.egl_display");
        const EGLint attributes[] = {EGL_SURFACE_TYPE,
                                     EGL_PBUFFER_BIT,
                                     EGL_RENDERABLE_TYPE,
                                     EGL_OPENGL_ES3_BIT,
                                     EGL_RED_SIZE,
                                     8,
                                     EGL_GREEN_SIZE,
                                     8,
                                     EGL_BLUE_SIZE,
                                     8,
                                     EGL_ALPHA_SIZE,
                                     8,
                                     EGL_NONE};
        EGLConfig config = nullptr;
        EGLint count = 0;
        if (!eglChooseConfig(display_, attributes, &config, 1, &count) || count != 1)
            throw std::runtime_error("probe.egl_config");
        const EGLint dimensions[] = {EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE};
        surface_ = eglCreatePbufferSurface(display_, config, dimensions);
        const EGLint context_attributes[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
        context_ = eglCreateContext(display_, config, EGL_NO_CONTEXT, context_attributes);
        if (surface_ == EGL_NO_SURFACE || context_ == EGL_NO_CONTEXT ||
            !eglMakeCurrent(display_, surface_, surface_, context_))
            throw std::runtime_error("probe.egl_context");
    }
    std::uintptr_t Context() const { return reinterpret_cast<std::uintptr_t>(context_); }

   private:
    // EGL handles are owned and confined to this test-only native adapter.
    EGLDisplay display_ = EGL_NO_DISPLAY;
    EGLSurface surface_ = EGL_NO_SURFACE;
    EGLContext context_ = EGL_NO_CONTEXT;
};
}  // namespace
class Host final {
   public:
    static render::Renderer CreateRenderer() {
        auto owner = std::make_shared<EglSurface>();
        owner->Initialize();
        const auto context = owner->Context();
        return render::Renderer(render::detail::CreateBgfxBackend(0, {16, 16}, owner, context));
    }
};
}  // namespace rhythm::platform
namespace {
rhythm::render::DrawList Quad(rhythm::render::TextureHandle texture, std::uint32_t top = 0xffffffff,
                              std::uint32_t bottom = 0xffffffff) {
    rhythm::render::DrawList list;
    list.width_ = 16;
    list.height_ = 16;
    list.vertices_ = {
            {0, 0, 0, 0, top}, {16, 0, 1, 0, top}, {16, 16, 1, 1, bottom}, {0, 16, 0, 1, bottom}};
    list.indices_ = {0, 1, 2, 0, 2, 3};
    list.commands_ = {{texture, 0, 6, {0, 0, 16, 16}}};
    return list;
}
void VerifySpectrum(rhythm::render::Renderer& renderer) {
    using namespace rhythm;
    graph::Registry registry;
    graph::Document document;
    document.id_ = "probe.audio_spectrum";
    document.output_ = 4;
    document.nodes_ = {
            registry.MakeNode(1, "texture.spectrum"), registry.MakeNode(2, "texture.gradient"),
            registry.MakeNode(3, "texture.blend"), registry.MakeNode(4, "output.texture")};
    document.edges_ = {{1, 2, 3, "a"}, {2, 1, 3, "b"}, {3, 3, 4, "source"}};
    document.nodes_[0].properties_["bar_count"] = 8.0;
    document.nodes_[0].properties_["bar_gap"] = 0.0;
    document.nodes_[0].properties_["color_a"] = graph::Color{1, 0, 0, 1};
    document.nodes_[0].properties_["color_b"] = graph::Color{1, 0, 0, 1};
    document.nodes_[1].properties_["color_a"] = graph::Color{0, 1, 0, 1};
    document.nodes_[1].properties_["color_b"] = graph::Color{0, 1, 0, 1};
    document.nodes_[2].properties_["amount"] = 1.0;
    runtime::FrameContext context;
    context.extent_ = {16, 16};
    context.external_.audio_ = audio::Features{};
    context.external_.audio_->valid_ = true;
    context.external_.audio_->sample_rate_ = 48000;
    context.external_.audio_->generation_ = 1;
    runtime::Runtime runtime;
    for (const auto layout : {0.0, 1.0}) {
        document.nodes_[0].properties_["spectrum_layout"] = layout;
        const auto plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
        for (const auto amplitude : {0.0f, 0.5f, 0.0f}) {
            context.external_.audio_->mono_bands_.fill(amplitude);
            for (int frame = 0; frame < 4; ++frame) {
                renderer.BeginFrame();
                const auto output = runtime.Evaluate(plan, context, renderer);
                renderer.Submit({}, Quad(output.final_));
                renderer.EndFrame();
            }
            std::array<std::uint8_t, 16 * 16 * 4> pixels{};
            glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
            glReadPixels(0, 0, 16, 16, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            int red = 0, green = 0;
            for (std::size_t index = 0; index < pixels.size(); index += 4) {
                red += pixels[index] > 200 && pixels[index + 1] < 30;
                green += pixels[index + 1] > 200 && pixels[index] < 30;
            }
            if (glGetError() != GL_NO_ERROR || green == 0 ||
                (amplitude == 0 ? green != 256 : red < 40))
                throw std::runtime_error("probe.audio_spectrum_composite");
        }
    }
    std::cout << "Audio spectrum GPU pixels: linear/radial, transparent composite, silence reset\n";
}
void VerifyAlpha(rhythm::render::Renderer& renderer) {
    using namespace rhythm;
    const std::array<std::uint8_t, 4> red{255, 0, 0, 128};
    const std::array<std::uint8_t, 4> white{255, 255, 255, 255};
    auto translucent = renderer.CreateTexture({1, 1}, red);
    auto opaque = renderer.CreateTexture({1, 1}, white);
    auto target = renderer.CreateTexture({16, 16});
    auto intermediate = renderer.CreateTexture({16, 16});
    for (int scenario = 0; scenario < 3; ++scenario) {
        for (int frame = 0; frame < 4; ++frame) {
            renderer.BeginFrame();
            if (scenario == 0)
                renderer.Submit(target.Handle(), Quad(translucent.Handle()));
            else if (scenario == 1)
                renderer.Submit(target.Handle(), Quad(opaque.Handle(), 0x800000ff, 0x800000ff));
            else {
                render::DrawList empty;
                empty.width_ = empty.height_ = 16;
                renderer.Submit(target.Handle(), empty, 0xff000080);
            }
            renderer.Submit(intermediate.Handle(), Quad(target.Handle()));
            renderer.Submit({}, Quad(intermediate.Handle()), 0x00ff00ff);
            renderer.EndFrame();
        }
        std::array<std::uint8_t, 4> pixel{};
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glReadPixels(8, 8, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
        if (glGetError() != GL_NO_ERROR || pixel[0] < 126 || pixel[0] > 130 || pixel[1] < 125 ||
            pixel[1] > 129 || pixel[2] > 1 || pixel[3] != 255)
            throw std::runtime_error("probe.multipass_alpha");
    }
    std::cout << "Translucent texture, vertex and clear retain coverage through three GPU passes\n";
}
void VerifyColor(rhythm::render::Renderer& renderer) {
    using namespace rhythm;
    const std::array<std::uint8_t, 4> rgba{128, 64, 32, 255};
    auto source = renderer.CreateTexture({1, 1}, rgba);
    auto target = renderer.CreateTexture({16, 16});
    const std::array adjustments{render::ColorAdjustment{}, render::ColorAdjustment{0, 1, 0, 0},
                                 render::ColorAdjustment{1, 1, 1, 0},
                                 render::ColorAdjustment{0, 0, 1, 0},
                                 render::ColorAdjustment{0, 1, 1, 1}};
    const std::array<std::array<int, 3>, 5> expected{
            {{128, 64, 32}, {75, 75, 75}, {255, 128, 64}, {128, 128, 128}, {127, 191, 223}}};
    for (std::size_t index = 0; index < adjustments.size(); ++index) {
        for (int frame = 0; frame < 4; ++frame) {
            renderer.BeginFrame();
            auto draw = Quad(source.Handle());
            draw.commands_[0].color_adjustment_ = adjustments[index];
            renderer.Submit(target.Handle(), draw);
            renderer.Submit({}, Quad(target.Handle()));
            renderer.EndFrame();
        }
        std::array<std::uint8_t, 4> pixel{};
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glReadPixels(8, 8, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
        for (std::size_t channel = 0; channel < 3; ++channel)
            if (std::abs(int(pixel[channel]) - expected[index][channel]) > 2)
                throw std::runtime_error("probe.color_filter_pixels");
        if (glGetError() != GL_NO_ERROR || pixel[3] != 255)
            throw std::runtime_error("probe.color_filter_gl");
    }
    const std::array<std::uint8_t, 4> red{255, 0, 0, 128};
    auto translucent = renderer.CreateTexture({1, 1}, red);
    for (int frame = 0; frame < 4; ++frame) {
        renderer.BeginFrame();
        auto draw = Quad(translucent.Handle());
        draw.commands_[0].color_adjustment_ = render::ColorAdjustment{0, 1, 0, 0};
        renderer.Submit(target.Handle(), draw);
        renderer.Submit({}, Quad(target.Handle()), 0x00ff00ff);
        renderer.EndFrame();
    }
    std::array<std::uint8_t, 4> pixel{};
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glReadPixels(8, 8, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
    if (glGetError() != GL_NO_ERROR || std::abs(int(pixel[0]) - 27) > 2 ||
        std::abs(int(pixel[1]) - 154) > 2 || std::abs(int(pixel[2]) - 27) > 2 || pixel[3] != 255)
        throw std::runtime_error("probe.color_filter_alpha");
    std::cout << "Color filter GPU pixels: identity, exposure, contrast, saturation, invert and "
                 "alpha\n";
}
void VerifyMask(rhythm::render::Renderer& renderer) {
    using namespace rhythm;
    graph::Registry registry;
    graph::Document document;
    document.id_ = "probe.mask";
    document.output_ = 6;
    document.nodes_ = {
            registry.MakeNode(1, "texture.shape"),     registry.MakeNode(2, "texture.shape"),
            registry.MakeNode(3, "texture.mask"),      registry.MakeNode(4, "texture.gradient"),
            registry.MakeNode(5, "texture.composite"), registry.MakeNode(6, "output.texture"),
            registry.MakeNode(7, "scalar.constant")};
    document.edges_ = {{1, 1, 3, "source"}, {2, 2, 3, "mask"},   {3, 4, 5, "a"},
                       {4, 3, 5, "b"},      {5, 5, 6, "source"}, {6, 7, 5, "amount"}};
    for (std::size_t index = 0; index < 2; ++index) {
        document.nodes_[index].properties_["shape_type"] = 0.0;
        document.nodes_[index].properties_["shape_height"] = 1.0;
        document.nodes_[index].properties_["shape_width"] = index == 0 ? 1.0 : 0.5;
    }
    document.nodes_[0].properties_["color_a"] = graph::Color{1, 0, 0, 1};
    for (const auto key : {"color_a", "color_b"})
        document.nodes_[3].properties_[key] = graph::Color{0, 1, 0, 1};
    document.nodes_[6].properties_["value"] = 0.5;
    runtime::Runtime runtime;
    runtime::FrameContext context;
    context.extent_ = {16, 16};
    for (const auto inverse : {0.0, 1.0}) {
        document.nodes_[2].properties_["mask_mode"] = inverse;
        for (const auto additive : {0.0, 1.0}) {
            document.nodes_[4].properties_["composite_mode"] = additive;
            const auto plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
            for (int frame = 0; frame < 4; ++frame) {
                renderer.BeginFrame();
                const auto output = runtime.Evaluate(plan, context, renderer);
                renderer.Submit({}, Quad(output.final_));
                renderer.EndFrame();
            }
            std::array<std::uint8_t, 16 * 16 * 4> pixels{};
            glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
            glReadPixels(0, 0, 16, 16, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            for (int y = 0; y < 16; ++y) {
                for (int x = 0; x < 16; ++x) {
                    const auto index = static_cast<std::size_t>((y * 16 + x) * 4);
                    const bool visible = (x >= 4 && x < 12) != (inverse == 1);
                    const int red = pixels[index], green = pixels[index + 1];
                    if (std::abs(red - (visible ? 128 : 0)) > 1 ||
                        std::abs(green - (visible && additive == 0 ? 127 : 255)) > 1)
                        throw std::runtime_error("probe.mask_composite_pixels");
                }
            }
            if (glGetError() != GL_NO_ERROR) throw std::runtime_error("probe.mask_gl_error");
        }
    }
    std::cout << "Shape/mask GPU pixels: normal/inverse alpha, source-over/additive, scalar "
                 "opacity\n";
}
void VerifyAffine(rhythm::render::Renderer& renderer) {
    using namespace rhythm;
    graph::Registry registry;
    graph::Document document;
    document.id_ = "probe.affine";
    document.output_ = 5;
    document.nodes_ = {
            registry.MakeNode(1, "texture.gradient"), registry.MakeNode(2, "texture.affine"),
            registry.MakeNode(3, "texture.gradient"), registry.MakeNode(4, "texture.blend"),
            registry.MakeNode(5, "output.texture"),   registry.MakeNode(6, "scalar.constant")};
    document.edges_ = {{1, 1, 2, "source"},
                       {2, 3, 4, "a"},
                       {3, 2, 4, "b"},
                       {4, 4, 5, "source"},
                       {5, 6, 2, "scale"}};
    for (const auto key : {"color_a", "color_b"}) {
        document.nodes_[0].properties_[key] = graph::Color{1, 0, 0, 1};
        document.nodes_[2].properties_[key] = graph::Color{0, 1, 0, 1};
    }
    document.nodes_[1].properties_["opacity"] = 0.5;
    document.nodes_[1].properties_["translate_x"] = 0.25;
    document.nodes_[1].properties_["rotation"] = 90.0;
    document.nodes_[3].properties_["amount"] = 1.0;
    runtime::Runtime runtime;
    runtime::FrameContext context;
    context.extent_ = {16, 16};
    for (const auto scale : {0.5, 0.0}) {
        document.nodes_[5].properties_["value"] = scale;
        const auto plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
        for (int frame = 0; frame < 4; ++frame) {
            renderer.BeginFrame();
            const auto output = runtime.Evaluate(plan, context, renderer);
            renderer.Submit({}, Quad(output.final_));
            renderer.EndFrame();
        }
        std::array<std::uint8_t, 16 * 16 * 4> pixels{};
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glReadPixels(0, 0, 16, 16, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        for (int y = 0; y < 16; ++y) {
            for (int x = 0; x < 16; ++x) {
                const auto index = static_cast<std::size_t>((y * 16 + x) * 4);
                const bool inside = scale > 0 && x >= 8 && y >= 4 && y < 12;
                const auto red = pixels[index], green = pixels[index + 1];
                if (inside ? red < 126 || red > 130 || green < 125 || green > 129
                           : red != 0 || green != 255)
                    throw std::runtime_error("probe.affine_pixels");
            }
        }
        if (glGetError() != GL_NO_ERROR) throw std::runtime_error("probe.affine_gl_error");
    }
    std::cout << "Affine GPU pixels: scalar scale, rotation, translation, opacity and zero scale\n";
}
}  // namespace
int main(int argc, char* argv[]) {
    using namespace rhythm;
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--execution-probe") {
            std::array<std::uint8_t, 32 * 16 * 4> pixels{};
            auto renderer = platform::Host::CreateRenderer();
            validation::VerifyGpuExecution(pixels);
            validation::VerifySceneInstances(renderer);
            validation::VerifyGpuParticles(renderer);
            validation::VerifyColorPipeline(renderer);
            validation::VerifySampleableDepth(renderer);
            return 0;
        }
#ifdef RHYTHM_HAS_LOCAL_MEDIA
        if (argc == 3 && (std::string_view(argv[2]) == "--music" ||
                          std::string_view(argv[2]) == "--arrangement")) {
            auto renderer = platform::Host::CreateRenderer();
            validation::VerifyMusicPackage(renderer, argv[1],
                                           std::string_view(argv[2]) == "--arrangement");
            return 0;
        }
#endif
        std::cout << std::unitbuf;
        if (argc == 2 && std::string_view(argv[1]) == "--readback") {
            auto renderer = platform::Host::CreateRenderer();
            if (renderer.SupportsReadback()) {
                rhythm::validation::VerifyReadbackPixels(renderer);
            } else {
                std::cout
                        << "readback: backend reports unsupported; no software fallback enabled\n";
            }
            return 0;
        }
        if (argc == 3 && (std::string_view(argv[2]) == "--benchmark" ||
                          std::string_view(argv[2]) == "--benchmark-compact" ||
                          std::string_view(argv[2]) == "--benchmark-balanced")) {
            auto renderer = platform::Host::CreateRenderer();
            const auto quality = std::string_view(argv[2]) == "--benchmark-compact"
                                         ? player::RenderQuality::kEconomy
                                 : std::string_view(argv[2]) == "--benchmark-balanced"
                                         ? player::RenderQuality::kBalanced
                                         : player::RenderQuality::kOriginal;
            rhythm::validation::MeasureTemplate(renderer, argv[1], quality);
            return 0;
        }
        for (int device = 0; device < 2; ++device) {
            auto renderer = platform::Host::CreateRenderer();
            const auto gpu = glGetString(GL_RENDERER);
            if (gpu) std::cout << "GPU " << gpu << '\n';
            VerifySpectrum(renderer);
            rhythm::validation::VerifyVideoUploadPixels(renderer);
            rhythm::validation::VerifyEffectPixels(renderer);
            rhythm::validation::VerifyPointPixels(renderer);
            rhythm::validation::VerifyTextureReusePixels(renderer);
            VerifyAlpha(renderer);
            VerifyAffine(renderer);
            VerifyMask(renderer);
            VerifyColor(renderer);
            const std::array<std::uint8_t, 4> white{255, 255, 255, 255};
            auto source = renderer.CreateTexture({1, 1}, white);
            auto target = renderer.CreateTexture({16, 16});
            auto intermediate = renderer.CreateTexture({16, 16});
            for (int scenario = 0; scenario < 3; ++scenario) {
                for (int frame = 0; frame < 4; ++frame) {
                    renderer.BeginFrame();
                    auto draw = Quad(source.Handle(), 0xff0000ff, 0xffff0000);
                    if (scenario == 2) draw.commands_[0].clip_.height_ = 8;
                    renderer.Submit(target.Handle(), draw, 0x000000ff);
                    if (scenario > 0) renderer.Submit(intermediate.Handle(), Quad(target.Handle()));
                    renderer.Submit({},
                                    Quad(scenario > 0 ? intermediate.Handle() : target.Handle()));
                    renderer.EndFrame();
                }
                std::array<std::uint8_t, 16 * 16 * 4> pixels{};
                glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
                glReadPixels(0, 0, 16, 16, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
                const auto top = (15 * 16 + 8) * 4;
                const auto bottom = 8 * 4;
                std::cout << "top=" << int(pixels[top]) << ',' << int(pixels[top + 2])
                          << " bottom=" << int(pixels[bottom]) << ',' << int(pixels[bottom + 2])
                          << '\n';
                if (glGetError() != GL_NO_ERROR || pixels[top] < 200 || pixels[top + 2] > 40 ||
                    pixels[bottom] > 40 ||
                    (scenario < 2 ? pixels[bottom + 2] < 200 : pixels[bottom + 2] > 10))
                    throw std::runtime_error("probe.texture_origin_or_pixels");
            }
            renderer.Invalidate();
            if (renderer.IsValid(target.Handle())) throw std::runtime_error("probe.invalidation");
        }
        if (argc > 3 || (argc == 3 && std::string_view(argv[2]) != "--template"))
            throw std::invalid_argument("probe.arguments");
        if (argc >= 2) {
            auto renderer = platform::Host::CreateRenderer();
            player::Session session;
            session.Open(argv[1]);
            for (int frame = 0; frame < 60; ++frame) {
                renderer.BeginFrame();
                const auto output = session.Tick(frame / 60.0, false, session.Canvas(), renderer);
                renderer.Submit({}, Quad(output.final_));
                renderer.EndFrame();
            }
            std::array<std::uint8_t, 16 * 16 * 4> pixels{};
            glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
            glReadPixels(0, 0, 16, 16, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            const auto top = (15 * 16 + 8) * 4;
            const auto bottom = 8 * 4;
            bool visible = false;
            for (std::size_t index = 0; index < pixels.size(); index += 4)
                visible |= pixels[index] > 20 || pixels[index + 1] > 20 || pixels[index + 2] > 20;
            const bool legacy_fixture = argc == 2;
            if (glGetError() != GL_NO_ERROR || !visible || renderer.Stats().passes_ < 2 ||
                (legacy_fixture &&
                 (pixels[top + 2] <= pixels[top] + 50 ||
                  pixels[bottom] <= pixels[bottom + 2] + 10 || renderer.Stats().passes_ < 4)))
                throw std::runtime_error("probe.published_package_output");
            std::cout << "Published Windows package: 60 Android GPU frames, time="
                      << session.Seconds() << " canvas=" << session.Canvas().width_ << "x"
                      << session.Canvas().height_ << '\n';
        }
        std::cout << "Android GLES multipass pixels and device recreation passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
