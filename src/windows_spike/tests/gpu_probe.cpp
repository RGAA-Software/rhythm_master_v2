#include <array>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>

#include "bgfx_handles.h"
#include "rhythm/platform/host.h"

namespace {
using rhythm::render::detail::GpuHandle;
GpuHandle<bgfx::ShaderHandle> LoadShader(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    const std::vector<char> bytes{std::istreambuf_iterator<char>(file),
                                  std::istreambuf_iterator<char>()};
    if (bytes.empty() || bytes.size() > 4 * 1024 * 1024)
        throw std::runtime_error("probe.shader_size");
    return GpuHandle(
            bgfx::createShader(bgfx::copy(bytes.data(), static_cast<std::uint32_t>(bytes.size()))));
}
void ComputeProbe(const std::filesystem::path& shader_path) {
    // Borrow the capability table only for this synchronous native boundary.
    const auto caps = bgfx::getCaps();
    if (caps) {
        std::cout << "capabilities: swap_chain=" << bool(caps->supported & BGFX_CAPS_SWAP_CHAIN)
                  << " transparent_backbuffer="
                  << bool(caps->supported & BGFX_CAPS_TRANSPARENT_BACKBUFFER) << '\n';
    }
    if (!caps ||
        (caps->supported &
         (BGFX_CAPS_COMPUTE | BGFX_CAPS_TEXTURE_BLIT | BGFX_CAPS_TEXTURE_READ_BACK)) !=
                (BGFX_CAPS_COMPUTE | BGFX_CAPS_TEXTURE_BLIT | BGFX_CAPS_TEXTURE_READ_BACK) ||
        !(caps->formats[bgfx::TextureFormat::RGBA8] & BGFX_CAPS_FORMAT_TEXTURE_IMAGE_WRITE))
        throw std::runtime_error("probe.compute_profile_unsupported");
    auto shader = LoadShader(shader_path);
    GpuHandle program(bgfx::createProgram(shader.Get(), false));
    GpuHandle texture(bgfx::createTexture2D(16, 16, false, 1, bgfx::TextureFormat::RGBA8,
                                            BGFX_TEXTURE_COMPUTE_WRITE));
    GpuHandle readback(bgfx::createTexture2D(16, 16, false, 1, bgfx::TextureFormat::RGBA8,
                                             BGFX_TEXTURE_READ_BACK | BGFX_TEXTURE_BLIT_DST));
    bgfx::setImage(0, texture.Get(), 0, bgfx::Access::Write, bgfx::TextureFormat::RGBA8);
    bgfx::dispatch(0, program.Get(), 2, 2, 1);
    bgfx::TextureRegion source;
    source.init(texture.Get());
    bgfx::TextureRegion destination;
    destination.init(readback.Get());
    bgfx::blit(1, destination, source);
    bgfx::frame();
    // Test-only native readback owns this fixed storage until its completion
    // frame. No application frame or viewer API exposes this borrowed buffer.
    std::array<std::uint8_t, 16 * 16 * 4> pixels{};
    const auto ready = bgfx::read(destination, pixels.data());
    auto completed = bgfx::frame();
    for (int wait = 0; completed < ready && wait < 16; ++wait) completed = bgfx::frame();
    if (completed < ready) std::terminate();  // Never free a pending native readback buffer.
    for (std::size_t pixel = 0; pixel < pixels.size(); pixel += 4) {
        const std::array<int, 4> expected{64, 128, 191, 255};
        for (std::size_t channel = 0; channel < 4; ++channel)
            if (std::abs(static_cast<int>(pixels[pixel + channel]) - expected[channel]) > 1)
                throw std::runtime_error("probe.compute_pixel_mismatch");
    }
    std::cout << "D3D11 Compute + blit + asynchronous readback: 256 RGBA pixels matched\n";
}
}  // namespace
// C ABI arguments remain inside this validation executable's host boundary.
int main(int argc, char* argv[]) {
    try {
        if (argc != 2) throw std::invalid_argument("probe.arguments");
        rhythm::platform::Host host(true);
        auto renderer = host.CreateRenderer();
        ComputeProbe(argv[1]);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
