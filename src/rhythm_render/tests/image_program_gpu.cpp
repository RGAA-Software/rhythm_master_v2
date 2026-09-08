#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include "gpu_execution_probe.h"
#include "rhythm/render/renderer.h"

namespace rhythm::validation {
void VerifyImageProgram(render::Renderer& renderer, const std::filesystem::path& path) {
    using namespace render;
    const auto size = std::filesystem::file_size(path);
    if (size > 512 * 1024) throw std::length_error("image_program.fixture_size");
    std::vector<std::uint8_t> bytes(std::size_t(size), 0);
    std::ifstream file(path, std::ios::binary);
    file.read(reinterpret_cast<char*>(bytes.data()), std::streamsize(bytes.size()));
    if (!file) throw std::runtime_error("image_program.fixture_read");
    auto program = renderer.CreateImageProgram(bytes);
    const auto original = program.Handle();
    const std::array<std::uint8_t, 4> source_pixel{0, 128, 255, 128};
    auto source = renderer.CreateTexture({1, 1}, source_pixel);
    auto target = renderer.CreateTexture({32, 16});
    DrawList list;
    list.width_ = 32;
    list.height_ = 16;
    list.vertices_ = {{0, 0, 0, 0}, {32, 0, 1, 0}, {32, 16, 1, 1}, {0, 16, 0, 1}};
    list.indices_ = {0, 1, 2, 0, 2, 3};
    list.commands_ = {{source.Handle(), 0, 6, {0, 0, 32, 16}}};
    list.commands_[0].image_program_ = ImageProgramInput{program.Handle()};
    for (int scenario = 0; scenario < 4; ++scenario) {
        auto& input = *list.commands_[0].image_program_;
        input.parameters_[0] = scenario ? 1.0f : 0.0f;
        input.seconds_ = scenario < 2 ? 0.0f : scenario == 2 ? 1.5707963f : 100000.0f;
        renderer.BeginFrame();
        renderer.Submit(target.Handle(), list);
        auto ticket = renderer.RequestReadback(target.Handle());
        renderer.EndFrame();
        std::optional<ReadbackImage> image;
        for (int wait = 0; wait < 32 && !image; ++wait) {
            image = ticket.Poll();
            if (!image) {
                renderer.BeginFrame();
                renderer.EndFrame();
            }
        }
        if (!image) throw std::runtime_error("image_program.readback_timeout");
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 32; ++x) {
                const auto offset = std::size_t(y * 32 + x) * 4;
                const std::array<float, 4> expected =
                        scenario == 0
                                ? std::array<float, 4>{0, 64, 128, 128}
                                : std::array<float, 4>{
                                          (x + 0.5f) / 32 * 255, (y + 0.5f) / 16 * 255,
                                          (std::sin(input.seconds_) * 0.5f + 0.5f) * 255, 255};
                for (std::size_t channel = 0; channel < 4; ++channel)
                    if (std::abs(float(image->rgba_[offset + channel]) - expected[channel]) > 2)
                        throw std::runtime_error("image_program.pixel_contract");
            }
        if (renderer.Stats().image_programs_ != 1 || program.Handle() != original)
            throw std::runtime_error("image_program.uniform_reuse");
    }
    auto broken = bytes;
    broken[20] = 255;
    bool rejected = false;
    try {
        auto candidate = renderer.CreateImageProgram(broken);
        program = std::move(candidate);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    if (!rejected || program.Handle() != original || !renderer.IsValid(original))
        throw std::runtime_error("image_program.failed_replacement");
    program = {};
    if (renderer.IsValid(original) || renderer.Stats().image_programs_ != 0 ||
        renderer.Stats().image_program_bytes_ != 0)
        throw std::runtime_error("image_program.release");
    std::cout << "Image program GPU: source alpha, top-left UV, parameters, high precision time, "
                 "cache and failed replacement passed\n";
}
}  // namespace rhythm::validation
