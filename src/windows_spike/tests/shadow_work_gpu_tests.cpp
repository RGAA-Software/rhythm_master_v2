#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "rhythm/assets/store.h"
#include "rhythm/graph/compiler.h"
#include "rhythm/platform/host.h"
#include "rhythm/prepared_assets/prepare.h"
#include "rhythm/project/store.h"
#include "rhythm/runtime/runtime.h"

namespace {
using Clock = std::chrono::steady_clock;
constexpr rhythm::render::Extent kExtent{640, 360};

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

rhythm::graph::ExecutionPlan Compile(rhythm::graph::Document document, bool shadows,
                                     std::size_t& shadow_nodes) {
    shadow_nodes = 0;
    for (auto& node : document.nodes_) {
        if (node.type_ != "scene.shadow") continue;
        ++shadow_nodes;
        node.properties_["shadow_enabled"] = shadows ? 1.0 : 0.0;
        node.properties_["shadow_filter"] = 2.0;
    }
    const auto compiled = rhythm::graph::Compile(document, rhythm::graph::Registry{});
    if (std::holds_alternative<std::vector<rhythm::graph::Diagnostic>>(compiled))
        throw std::runtime_error("shadow_work.compile");
    return std::get<rhythm::graph::ExecutionPlan>(compiled);
}

std::vector<rhythm::project::PackagedAsset> VisualAssets(
        const std::filesystem::path& directory,
        std::span<const rhythm::assets::AssetRecord> records) {
    rhythm::assets::Store store(directory / "assets");
    std::vector<rhythm::project::PackagedAsset> result;
    for (const auto& record : records) {
        if (std::string_view(record.media_type_).starts_with("audio/")) continue;
        result.push_back({record, store.Read(record)});
    }
    return result;
}

rhythm::runtime::FrameContext Context(double seconds,
                                      const rhythm::prepared_assets::Resources& resources) {
    rhythm::runtime::FrameContext context;
    context.seconds_ = seconds;
    context.reset_generation_ = 1;
    context.extent_ = kExtent;
    context.evaluate_viewers_ = false;
    context.resources_ = resources.models_;
    context.images_ = resources.images_;
    context.shaders_ = resources.shaders_;
    context.surfaces_ = resources.surfaces_;
    context.retained_textures_ = std::vector<rhythm::graph::NodeId>{};
    return context;
}

std::vector<std::uint8_t> Readback(rhythm::render::Renderer& renderer,
                                   rhythm::render::Readback ticket) {
    for (int frame = 0; frame < 32; ++frame) {
        if (auto image = ticket.Poll()) return image->rgba_;
        renderer.BeginFrame();
        renderer.EndFrame();
    }
    throw std::runtime_error("shadow_work.readback_timeout");
}

double MeanRgbDifference(std::span<const std::uint8_t> first,
                         std::span<const std::uint8_t> second) {
    Require(first.size() == second.size(), "shadow_work.readback_size");
    std::uint64_t difference = 0;
    for (std::size_t offset = 0; offset < first.size(); offset += 4) {
        difference += std::uint64_t(std::abs(int(first[offset]) - int(second[offset])));
        difference += std::uint64_t(std::abs(int(first[offset + 1]) - int(second[offset + 1])));
        difference += std::uint64_t(std::abs(int(first[offset + 2]) - int(second[offset + 2])));
    }
    return double(difference) / (double(first.size() / 4) * 3.0);
}

void WritePpm(const std::filesystem::path& path, std::span<const std::uint8_t> rgba) {
    Require(rgba.size() == std::size_t(kExtent.width_) * kExtent.height_ * 4,
            "shadow_work.ppm_size");
    std::ofstream output(path, std::ios::binary);
    output.exceptions(std::ios::badbit | std::ios::failbit);
    output << "P6\n" << kExtent.width_ << ' ' << kExtent.height_ << "\n255\n";
    for (std::size_t offset = 0; offset < rgba.size(); offset += 4) {
        output.put(char(rgba[offset]));
        output.put(char(rgba[offset + 1]));
        output.put(char(rgba[offset + 2]));
    }
}

struct MotionResult {
    std::vector<std::uint8_t> final_{};
    std::vector<double> differences_{};
    std::optional<rhythm::scene::Camera> first_camera_{};
    std::optional<rhythm::scene::Camera> final_camera_{};
    std::uint64_t texture_bytes_ = 0;
};

MotionResult CaptureMotion(const rhythm::graph::ExecutionPlan& plan,
                           const rhythm::prepared_assets::Resources& resources,
                           rhythm::render::Renderer& renderer) {
    rhythm::runtime::Runtime runtime;
    MotionResult summary;
    std::vector<std::uint8_t> previous;
    for (int frame = 0; frame <= 120; ++frame) {
        renderer.BeginFrame();
        const auto result = runtime.Evaluate(plan, Context(frame / 30.0, resources), renderer);
        Require(!result.budget_ && renderer.IsValid(result.final_),
                "shadow_work.current_output_invalid");
        auto ticket = renderer.RequestReadback(result.final_);
        if (frame == 0 || frame == 120) {
            for (const auto& output : result.outputs_) {
                if (!output.camera_) continue;
                if (frame == 0) summary.first_camera_ = output.camera_;
                if (frame == 120) summary.final_camera_ = output.camera_;
            }
        }
        renderer.EndFrame();
        auto image = Readback(renderer, std::move(ticket));
        if (!previous.empty()) summary.differences_.push_back(MeanRgbDifference(previous, image));
        previous = std::move(image);
        if (frame == 30) summary.texture_bytes_ = renderer.Stats().texture_bytes_;
        if (frame > 30 && renderer.Stats().texture_bytes_ != summary.texture_bytes_)
            throw std::runtime_error("shadow_work.texture_growth");
    }
    summary.final_ = std::move(previous);
    return summary;
}

struct TimingResult {
    double p50_ = 0;
    double p95_ = 0;
};

TimingResult Measure(const rhythm::graph::ExecutionPlan& plan,
                     const rhythm::prepared_assets::Resources& resources,
                     rhythm::render::Renderer& renderer) {
    rhythm::runtime::Runtime runtime;
    std::vector<double> timings;
    for (int frame = 0; frame < 150; ++frame) {
        const auto begin = Clock::now();
        renderer.BeginFrame();
        const auto result = runtime.Evaluate(plan, Context(frame / 30.0, resources), renderer);
        Require(!result.budget_ && renderer.IsValid(result.final_),
                "shadow_work.performance_output_invalid");
        renderer.EndFrame();
        if (frame >= 30)
            timings.push_back(
                    std::chrono::duration<double, std::milli>(Clock::now() - begin).count());
    }
    std::sort(timings.begin(), timings.end());
    return {timings[timings.size() / 2], timings[timings.size() * 95 / 100]};
}

double CameraDistance(const rhythm::scene::Camera& first, const rhythm::scene::Camera& second) {
    const auto square = [](double value) { return value * value; };
    return std::sqrt(square(first.eye_.x_ - second.eye_.x_) +
                     square(first.eye_.y_ - second.eye_.y_) +
                     square(first.eye_.z_ - second.eye_.z_));
}
}  // namespace

int main(int argc, char* argv[]) {
    using namespace rhythm;
    try {
        if (argc != 3) throw std::invalid_argument("template output");
        const std::filesystem::path source(argv[1]), output(argv[2]);
        std::filesystem::create_directories(output);
        const auto loaded = project::LoadRevision(source);
        std::size_t shadow_nodes = 0;
        const auto shadow_plan = Compile(loaded.snapshot_.document_, true, shadow_nodes);
        std::size_t disabled_shadow_nodes = 0;
        const auto unshadowed_plan =
                Compile(loaded.snapshot_.document_, false, disabled_shadow_nodes);
        Require(shadow_nodes == 1 && disabled_shadow_nodes == 1,
                "shadow_work.expected_one_shadow_node");
        const auto assets = VisualAssets(source, loaded.snapshot_.assets_);
        const auto resources = prepared_assets::Prepare(shadow_plan, assets);

        platform::Host host(true);
        host.Resize(kExtent);
        auto renderer = host.CreateRenderer();
        const auto motion = CaptureMotion(shadow_plan, *resources, renderer);
        const auto no_shadow = CaptureMotion(unshadowed_plan, *resources, renderer);
        Require(motion.first_camera_ && motion.final_camera_, "shadow_work.camera_missing");
        const auto camera_distance = CameraDistance(*motion.first_camera_, *motion.final_camera_);
        Require(camera_distance > 1.0, "shadow_work.camera_did_not_travel");
        const auto shadow_difference = MeanRgbDifference(motion.final_, no_shadow.final_);
        WritePpm(output / "pcf13-final.ppm", motion.final_);
        WritePpm(output / "no-shadow-final.ppm", no_shadow.final_);
        std::cout << "Shadow work precheck: camera_distance=" << camera_distance
                  << " shadow_difference=" << shadow_difference << "\n";
        Require(shadow_difference > 0.25, "shadow_work.shadow_not_visible");
        const auto sorted_differences = [&] {
            auto result = motion.differences_;
            std::sort(result.begin(), result.end());
            return result;
        }();
        const auto motion_p50 = sorted_differences[sorted_differences.size() / 2];
        const auto motion_p95 = sorted_differences[sorted_differences.size() * 95 / 100];
        const auto motion_max = sorted_differences.back();
        Require(motion_max < std::max(10.0, motion_p95 * 1.5), "shadow_work.visible_motion_pop");
        const auto high_timing = Measure(shadow_plan, *resources, renderer);
        const auto no_shadow_timing = Measure(unshadowed_plan, *resources, renderer);
        std::ifstream graph(source / "graph.pb", std::ios::binary);
        graph.exceptions(std::ios::badbit | std::ios::failbit);
        const std::string graph_bytes{std::istreambuf_iterator<char>(graph), {}};
        std::ofstream evidence(output / "results.json");
        evidence.exceptions(std::ios::badbit | std::ios::failbit);
        evidence << "{\n"
                 << "    \"compiled_graph_sha256\": \"" << project::Digest(graph_bytes) << "\",\n"
                 << "    \"frames\": 121,\n"
                 << "    \"extent\": [640, 360],\n"
                 << "    \"camera_distance\": " << camera_distance << ",\n"
                 << "    \"shadow_mean_rgb_difference\": " << shadow_difference << ",\n"
                 << "    \"motion_difference_p50\": " << motion_p50 << ",\n"
                 << "    \"motion_difference_p95\": " << motion_p95 << ",\n"
                 << "    \"motion_difference_max\": " << motion_max << ",\n"
                 << "    \"pcf13_frame_p50_ms\": " << high_timing.p50_ << ",\n"
                 << "    \"pcf13_frame_p95_ms\": " << high_timing.p95_ << ",\n"
                 << "    \"no_shadow_frame_p50_ms\": " << no_shadow_timing.p50_ << ",\n"
                 << "    \"no_shadow_frame_p95_ms\": " << no_shadow_timing.p95_ << ",\n"
                 << "    \"stable_texture_bytes\": " << motion.texture_bytes_ << "\n"
                 << "}\n";
        std::cout << "Shadow work: camera_distance=" << camera_distance
                  << " shadow_difference=" << shadow_difference << " motion=" << motion_p50 << '/'
                  << motion_p95 << '/' << motion_max << " pcf13_ms=" << high_timing.p50_ << '/'
                  << high_timing.p95_ << " no_shadow_ms=" << no_shadow_timing.p50_ << '/'
                  << no_shadow_timing.p95_ << "\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
