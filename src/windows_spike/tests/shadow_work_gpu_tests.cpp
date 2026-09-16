#include <bgfx/bgfx.h>

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
                                     std::size_t& shadow_nodes, double filter = 2,
                                     std::optional<double> extent = std::nullopt,
                                     std::uint8_t cascades = 1,
                                     std::optional<double> light = std::nullopt) {
    shadow_nodes = 0;
    for (auto& node : document.nodes_) {
        if (node.type_ != "scene.shadow") continue;
        ++shadow_nodes;
        node.properties_["shadow_enabled"] = shadows ? 1.0 : 0.0;
        node.properties_["shadow_filter"] = filter;
        node.properties_["shadow_cascades"] = double(cascades - 1);
        if (extent) node.properties_["shadow_extent"] = *extent;
        if (light) node.properties_["shadow_light"] = *light;
    }
    const auto compiled = rhythm::graph::Compile(document, rhythm::graph::Registry{});
    if (std::holds_alternative<std::vector<rhythm::graph::Diagnostic>>(compiled)) {
        const auto& diagnostics = std::get<std::vector<rhythm::graph::Diagnostic>>(compiled);
        throw std::runtime_error("shadow_work.compile." + diagnostics.at(0).code_);
    }
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
    struct Checkpoint {
        int frame_ = 0;
        std::vector<std::uint8_t> image_{};
    };
    std::vector<std::uint8_t> final_{};
    std::vector<double> differences_{};
    std::vector<Checkpoint> checkpoints_{};
    std::optional<rhythm::scene::Camera> first_camera_{};
    std::optional<rhythm::scene::Camera> final_camera_{};
    std::uint64_t texture_bytes_ = 0;
};

MotionResult CaptureMotion(const rhythm::graph::ExecutionPlan& plan,
                           const rhythm::prepared_assets::Resources& resources,
                           rhythm::render::Renderer& renderer, int final_frame = 120) {
    rhythm::runtime::Runtime runtime;
    MotionResult summary;
    std::vector<std::uint8_t> previous;
    for (int frame = 0; frame <= final_frame; ++frame) {
        renderer.BeginFrame();
        const auto result = runtime.Evaluate(plan, Context(frame / 30.0, resources), renderer);
        Require(!result.budget_ && renderer.IsValid(result.final_),
                "shadow_work.current_output_invalid");
        auto ticket = renderer.RequestReadback(result.final_);
        if (frame == 0 || frame == final_frame) {
            for (const auto& output : result.outputs_) {
                if (!output.camera_) continue;
                if (frame == 0) summary.first_camera_ = output.camera_;
                if (frame == final_frame) summary.final_camera_ = output.camera_;
            }
        }
        renderer.EndFrame();
        auto image = Readback(renderer, std::move(ticket));
        if (!previous.empty()) summary.differences_.push_back(MeanRgbDifference(previous, image));
        if (frame % 60 == 0 || frame == final_frame) summary.checkpoints_.push_back({frame, image});
        previous = std::move(image);
        if (frame == 30) summary.texture_bytes_ = renderer.Stats().texture_bytes_;
        if (frame > 30 && renderer.Stats().texture_bytes_ != summary.texture_bytes_)
            throw std::runtime_error("shadow_work.texture_growth");
    }
    summary.final_ = std::move(previous);
    return summary;
}

const std::vector<std::uint8_t>& Checkpoint(const MotionResult& result, int frame) {
    const auto found = std::find_if(result.checkpoints_.begin(), result.checkpoints_.end(),
                                    [frame](const auto& value) { return value.frame_ == frame; });
    if (found == result.checkpoints_.end()) throw std::runtime_error("shadow_work.checkpoint");
    return found->image_;
}

double MaximumCheckpointDifference(const MotionResult& first, const MotionResult& second,
                                   int final_frame) {
    double difference = 0;
    for (int frame = 0; frame <= final_frame; frame += 60)
        difference = std::max(
                difference, MeanRgbDifference(Checkpoint(first, frame), Checkpoint(second, frame)));
    return difference;
}

struct TimingResult {
    double p50_ = 0;
    double p95_ = 0;
    double gpu_p50_ = 0;
    double gpu_p95_ = 0;
    double depth_gpu_p50_ = 0;
    double depth_gpu_p95_ = 0;
    std::size_t depth_views_ = 0;
};

TimingResult Measure(const rhythm::graph::ExecutionPlan& plan,
                     const rhythm::prepared_assets::Resources& resources,
                     rhythm::render::Renderer& renderer) {
    rhythm::runtime::Runtime runtime;
    std::vector<double> timings;
    std::vector<double> gpu_timings;
    std::vector<double> depth_gpu_timings;
    std::optional<std::size_t> depth_views;
    for (int frame = 0; frame < 150; ++frame) {
        const auto begin = Clock::now();
        renderer.BeginFrame();
        const auto result = runtime.Evaluate(plan, Context(frame / 30.0, resources), renderer);
        Require(!result.budget_ && renderer.IsValid(result.final_),
                "shadow_work.performance_output_invalid");
        renderer.EndFrame();
        if (frame >= 30) {
            timings.push_back(
                    std::chrono::duration<double, std::milli>(Clock::now() - begin).count());
            // Borrowed backend timestamps remain inside this Windows diagnostic boundary.
            const auto* stats = bgfx::getStats();
            Require(stats && stats->gpuTimerFreq > 0, "shadow_work.gpu_timestamps_unavailable");
            const auto milliseconds = [stats](std::int64_t begin_ticks, std::int64_t end_ticks) {
                return 1000.0 * double(end_ticks - begin_ticks) / double(stats->gpuTimerFreq);
            };
            gpu_timings.push_back(milliseconds(stats->gpuTimeBegin, stats->gpuTimeEnd));
            double depth_gpu = 0;
            std::size_t current_depth_views = 0;
            for (std::size_t view_index = 0; view_index < stats->numViews; ++view_index) {
                const auto& view = stats->viewStats[view_index];
                if (std::string_view(view.name) != "Scene depth") continue;
                Require(view.gpuTimeEnd >= view.gpuTimeBegin,
                        "shadow_work.depth_gpu_timestamp_order");
                depth_gpu += milliseconds(view.gpuTimeBegin, view.gpuTimeEnd);
                ++current_depth_views;
            }
            if (!depth_views)
                depth_views = current_depth_views;
            else
                Require(*depth_views == current_depth_views,
                        "shadow_work.depth_gpu_view_count_changed");
            depth_gpu_timings.push_back(depth_gpu);
        }
    }
    const auto percentiles = [](std::vector<double>& values) {
        std::sort(values.begin(), values.end());
        return std::array{values[values.size() / 2], values[values.size() * 95 / 100]};
    };
    const auto host = percentiles(timings);
    const auto gpu = percentiles(gpu_timings);
    const auto depth_gpu = percentiles(depth_gpu_timings);
    return {host[0], host[1], gpu[0], gpu[1], depth_gpu[0], depth_gpu[1], depth_views.value_or(0)};
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
        if (argc != 5)
            throw std::invalid_argument("moving_template fine_template point_template output");
        const std::filesystem::path source(argv[1]), fine_source(argv[2]), point_source(argv[3]),
                output(argv[4]);
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

        const auto fine_loaded = project::LoadRevision(fine_source);
        std::size_t fine_shadow_nodes = 0;
        const auto fine_high_plan =
                Compile(fine_loaded.snapshot_.document_, true, fine_shadow_nodes, 2);
        std::size_t fine_low_shadow_nodes = 0;
        const auto fine_low_plan =
                Compile(fine_loaded.snapshot_.document_, true, fine_low_shadow_nodes, 1);
        std::size_t fine_disabled_shadow_nodes = 0;
        const auto fine_unshadowed_plan =
                Compile(fine_loaded.snapshot_.document_, false, fine_disabled_shadow_nodes, 2);
        std::size_t large_shadow_nodes = 0;
        const auto large_plan =
                Compile(fine_loaded.snapshot_.document_, true, large_shadow_nodes, 2, 36.0);
        std::size_t cascade_shadow_nodes = 0;
        const auto cascade_plan = Compile(fine_loaded.snapshot_.document_, true,
                                          cascade_shadow_nodes, 2, std::nullopt, 2);
        Require(fine_shadow_nodes == 1 && fine_low_shadow_nodes == 1 &&
                        fine_disabled_shadow_nodes == 1 && large_shadow_nodes == 1 &&
                        cascade_shadow_nodes == 1,
                "shadow_work.fine_expected_one_shadow_node");
        const auto fine_assets = VisualAssets(fine_source, fine_loaded.snapshot_.assets_);
        const auto fine_resources = prepared_assets::Prepare(fine_high_plan, fine_assets);
        const auto fine_high = CaptureMotion(fine_high_plan, *fine_resources, renderer, 480);
        const auto fine_low = CaptureMotion(fine_low_plan, *fine_resources, renderer);
        const auto fine_no_shadow = CaptureMotion(fine_unshadowed_plan, *fine_resources, renderer);
        const auto large = CaptureMotion(large_plan, *fine_resources, renderer);
        const auto cascade = CaptureMotion(cascade_plan, *fine_resources, renderer);
        const auto fine_shadow_difference =
                MaximumCheckpointDifference(fine_high, fine_no_shadow, 120);
        const auto filter_difference = MaximumCheckpointDifference(fine_high, fine_low, 120);
        const auto large_shadow_difference =
                MaximumCheckpointDifference(large, fine_no_shadow, 120);
        const auto cascade_shadow_difference =
                MaximumCheckpointDifference(cascade, fine_no_shadow, 120);
        const auto cascade_to_single_difference =
                MaximumCheckpointDifference(cascade, fine_high, 120);
        Require(fine_shadow_difference > 0.1, "shadow_work.fine_shadow_not_visible");
        Require(filter_difference > 0.01, "shadow_work.fine_filter_not_visible");
        Require(large_shadow_difference > 0.01, "shadow_work.large_shadow_not_visible");
        Require(cascade_shadow_difference > 0.1, "shadow_work.cascade_shadow_not_visible");
        Require(cascade_to_single_difference > 0.001,
                "shadow_work.cascade_does_not_change_authored_output");
        auto fine_differences = fine_high.differences_;
        std::sort(fine_differences.begin(), fine_differences.end());
        const auto fine_motion_p50 = fine_differences[fine_differences.size() / 2];
        const auto fine_motion_p95 = fine_differences[fine_differences.size() * 95 / 100];
        const auto fine_motion_max = fine_differences.back();
        const auto cycle_boundary = fine_high.differences_.back();
        Require(fine_motion_p50 > 0.1, "shadow_work.fine_motion_missing");
        Require(fine_motion_max < std::max(3.0, fine_motion_p95 * 2.0),
                "shadow_work.fine_motion_pop");
        Require(cycle_boundary < std::max(3.0, fine_motion_p95 * 2.0),
                "shadow_work.fine_cycle_boundary_pop");
        const auto fine_output = output / "chromatic-loom";
        std::filesystem::create_directories(fine_output);
        WritePpm(fine_output / "pcf13-frame120.ppm", Checkpoint(fine_high, 120));
        WritePpm(fine_output / "pcf5-frame120.ppm", Checkpoint(fine_low, 120));
        WritePpm(fine_output / "large-pcf13-frame120.ppm", Checkpoint(large, 120));
        WritePpm(fine_output / "cascade-pcf13-frame120.ppm", Checkpoint(cascade, 120));
        WritePpm(fine_output / "no-shadow-frame120.ppm", Checkpoint(fine_no_shadow, 120));
        const auto fine_timing = Measure(fine_high_plan, *fine_resources, renderer);
        const auto fine_low_timing = Measure(fine_low_plan, *fine_resources, renderer);
        const auto large_timing = Measure(large_plan, *fine_resources, renderer);
        const auto cascade_timing = Measure(cascade_plan, *fine_resources, renderer);
        const auto fine_no_shadow_timing = Measure(fine_unshadowed_plan, *fine_resources, renderer);
        std::ifstream fine_graph(fine_source / "graph.pb", std::ios::binary);
        fine_graph.exceptions(std::ios::badbit | std::ios::failbit);
        const std::string fine_graph_bytes{std::istreambuf_iterator<char>(fine_graph), {}};
        std::ofstream fine_evidence(fine_output / "results.json");
        fine_evidence.exceptions(std::ios::badbit | std::ios::failbit);
        fine_evidence << "{\n"
                      << "    \"compiled_graph_sha256\": \"" << project::Digest(fine_graph_bytes)
                      << "\",\n"
                      << "    \"frames\": 481,\n"
                      << "    \"extent\": [640, 360],\n"
                      << "    \"authored_shadow_extent\": 9,\n"
                      << "    \"large_shadow_extent\": 36,\n"
                      << "    \"shadow_mean_rgb_difference_max\": " << fine_shadow_difference
                      << ",\n"
                      << "    \"pcf5_to_pcf13_mean_rgb_difference_max\": " << filter_difference
                      << ",\n"
                      << "    \"large_shadow_mean_rgb_difference_max\": " << large_shadow_difference
                      << ",\n"
                      << "    \"cascade_shadow_mean_rgb_difference_max\": "
                      << cascade_shadow_difference << ",\n"
                      << "    \"cascade_to_single_mean_rgb_difference_max\": "
                      << cascade_to_single_difference << ",\n"
                      << "    \"motion_difference_p50\": " << fine_motion_p50 << ",\n"
                      << "    \"motion_difference_p95\": " << fine_motion_p95 << ",\n"
                      << "    \"motion_difference_max\": " << fine_motion_max << ",\n"
                      << "    \"frame_479_to_480_difference\": " << cycle_boundary << ",\n"
                      << "    \"pcf13_frame_p50_ms\": " << fine_timing.p50_ << ",\n"
                      << "    \"pcf13_frame_p95_ms\": " << fine_timing.p95_ << ",\n"
                      << "    \"pcf5_frame_p50_ms\": " << fine_low_timing.p50_ << ",\n"
                      << "    \"pcf5_frame_p95_ms\": " << fine_low_timing.p95_ << ",\n"
                      << "    \"large_pcf13_frame_p50_ms\": " << large_timing.p50_ << ",\n"
                      << "    \"large_pcf13_frame_p95_ms\": " << large_timing.p95_ << ",\n"
                      << "    \"cascade_pcf13_frame_p50_ms\": " << cascade_timing.p50_ << ",\n"
                      << "    \"cascade_pcf13_frame_p95_ms\": " << cascade_timing.p95_ << ",\n"
                      << "    \"no_shadow_frame_p50_ms\": " << fine_no_shadow_timing.p50_ << ",\n"
                      << "    \"no_shadow_frame_p95_ms\": " << fine_no_shadow_timing.p95_ << ",\n"
                      << "    \"stable_texture_bytes\": " << fine_high.texture_bytes_ << ",\n"
                      << "    \"cascade_stable_texture_bytes\": " << cascade.texture_bytes_ << "\n"
                      << "}\n";
        std::cout << "Fine shadow work: shadow=" << fine_shadow_difference
                  << " filters=" << filter_difference << " large=" << large_shadow_difference
                  << " cascade=" << cascade_shadow_difference << '/' << cascade_to_single_difference
                  << " motion=" << fine_motion_p50 << '/' << fine_motion_p95 << '/'
                  << fine_motion_max << " boundary=" << cycle_boundary
                  << " pcf13_ms=" << fine_timing.p50_ << '/' << fine_timing.p95_
                  << " pcf5_ms=" << fine_low_timing.p50_ << '/' << fine_low_timing.p95_
                  << " large_ms=" << large_timing.p50_ << '/' << large_timing.p95_
                  << " cascade_ms=" << cascade_timing.p50_ << '/' << cascade_timing.p95_
                  << " no_shadow_ms=" << fine_no_shadow_timing.p50_ << '/'
                  << fine_no_shadow_timing.p95_ << "\n";

        const auto point_loaded = project::LoadRevision(point_source);
        std::size_t point_shadow_nodes = 0;
        const auto point_plan = Compile(point_loaded.snapshot_.document_, true, point_shadow_nodes,
                                        2, std::nullopt, 1, 1);
        std::size_t point_disabled_nodes = 0;
        const auto point_disabled_plan =
                Compile(point_loaded.snapshot_.document_, false, point_disabled_nodes, 2);
        Require(point_shadow_nodes == 1 && point_disabled_nodes == 1,
                "shadow_work.point_expected_one_shadow_node");
        const auto point_assets = VisualAssets(point_source, point_loaded.snapshot_.assets_);
        const auto point_resources = prepared_assets::Prepare(point_plan, point_assets);
        const auto point = CaptureMotion(point_plan, *point_resources, renderer);
        const auto point_disabled = CaptureMotion(point_disabled_plan, *point_resources, renderer);
        const auto point_shadow_difference =
                MaximumCheckpointDifference(point, point_disabled, 120);
        Require(point_shadow_difference > 0.1, "shadow_work.point_shadow_not_visible");
        Require(point.texture_bytes_ == point_disabled.texture_bytes_ + 6ULL * 1024 * 1024 * 8,
                "shadow_work.point_shadow_texture_accounting");
        const auto point_output = output / "sonic-enamel";
        std::filesystem::create_directories(point_output);
        WritePpm(point_output / "point-pcf13-frame120.ppm", Checkpoint(point, 120));
        WritePpm(point_output / "no-shadow-frame120.ppm", Checkpoint(point_disabled, 120));
        bgfx::setDebug(BGFX_DEBUG_PROFILER);
        const auto point_timing = Measure(point_plan, *point_resources, renderer);
        const auto point_disabled_timing = Measure(point_disabled_plan, *point_resources, renderer);
        bgfx::setDebug(BGFX_DEBUG_NONE);
        std::cout << "Point GPU timing diagnostic: depth_views=" << point_timing.depth_views_ << '/'
                  << point_disabled_timing.depth_views_
                  << " depth_ms=" << point_timing.depth_gpu_p50_ << '/'
                  << point_timing.depth_gpu_p95_ << '/' << point_disabled_timing.depth_gpu_p50_
                  << '/' << point_disabled_timing.depth_gpu_p95_
                  << " frame_ms=" << point_timing.gpu_p50_ << '/' << point_timing.gpu_p95_ << '/'
                  << point_disabled_timing.gpu_p50_ << '/' << point_disabled_timing.gpu_p95_
                  << '\n';
        Require(point_timing.depth_views_ == point_disabled_timing.depth_views_ + 6 &&
                        point_timing.depth_gpu_p50_ > point_disabled_timing.depth_gpu_p50_ &&
                        point_timing.depth_gpu_p95_ > point_disabled_timing.depth_gpu_p95_,
                "shadow_work.point_gpu_depth_isolation");
        const auto depth_gpu_p50 =
                point_timing.depth_gpu_p50_ - point_disabled_timing.depth_gpu_p50_;
        const auto depth_gpu_p95 =
                point_timing.depth_gpu_p95_ - point_disabled_timing.depth_gpu_p95_;
        std::ofstream point_evidence(point_output / "results.json");
        point_evidence.exceptions(std::ios::badbit | std::ios::failbit);
        point_evidence << "{\n"
                       << "    \"frames\": 121,\n"
                       << "    \"extent\": [640, 360],\n"
                       << "    \"shadow_light\": 1,\n"
                       << "    \"point_shadow_mean_rgb_difference_max\": "
                       << point_shadow_difference << ",\n"
                       << "    \"point_pcf13_frame_p50_ms\": " << point_timing.p50_ << ",\n"
                       << "    \"point_pcf13_frame_p95_ms\": " << point_timing.p95_ << ",\n"
                       << "    \"point_gpu_frame_p50_ms\": " << point_timing.gpu_p50_ << ",\n"
                       << "    \"point_gpu_frame_p95_ms\": " << point_timing.gpu_p95_ << ",\n"
                       << "    \"point_scene_depth_gpu_p50_ms\": " << point_timing.depth_gpu_p50_
                       << ",\n"
                       << "    \"point_scene_depth_gpu_p95_ms\": " << point_timing.depth_gpu_p95_
                       << ",\n"
                       << "    \"point_shadow_depth_gpu_p50_delta_ms\": " << depth_gpu_p50 << ",\n"
                       << "    \"point_shadow_depth_gpu_p95_delta_ms\": " << depth_gpu_p95 << ",\n"
                       << "    \"point_shadow_depth_views\": "
                       << point_timing.depth_views_ - point_disabled_timing.depth_views_ << ",\n"
                       << "    \"no_shadow_frame_p50_ms\": " << point_disabled_timing.p50_ << ",\n"
                       << "    \"no_shadow_frame_p95_ms\": " << point_disabled_timing.p95_ << ",\n"
                       << "    \"no_shadow_gpu_frame_p50_ms\": " << point_disabled_timing.gpu_p50_
                       << ",\n"
                       << "    \"no_shadow_gpu_frame_p95_ms\": " << point_disabled_timing.gpu_p95_
                       << ",\n"
                       << "    \"no_shadow_scene_depth_gpu_p50_ms\": "
                       << point_disabled_timing.depth_gpu_p50_ << ",\n"
                       << "    \"no_shadow_scene_depth_gpu_p95_ms\": "
                       << point_disabled_timing.depth_gpu_p95_ << ",\n"
                       << "    \"point_stable_texture_bytes\": " << point.texture_bytes_ << ",\n"
                       << "    \"no_shadow_stable_texture_bytes\": "
                       << point_disabled.texture_bytes_ << "\n"
                       << "}\n";
        std::cout << "Point shadow work: difference=" << point_shadow_difference
                  << " point_ms=" << point_timing.p50_ << '/' << point_timing.p95_
                  << " point_gpu_ms=" << point_timing.gpu_p50_ << '/' << point_timing.gpu_p95_
                  << " depth_gpu_delta_ms=" << depth_gpu_p50 << '/' << depth_gpu_p95
                  << " no_shadow_ms=" << point_disabled_timing.p50_ << '/'
                  << point_disabled_timing.p95_
                  << " no_shadow_gpu_ms=" << point_disabled_timing.gpu_p50_ << '/'
                  << point_disabled_timing.gpu_p95_ << " texture_bytes=" << point.texture_bytes_
                  << '/' << point_disabled.texture_bytes_ << "\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
