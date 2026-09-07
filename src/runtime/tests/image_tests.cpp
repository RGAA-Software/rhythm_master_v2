#include <cmath>
#include <iostream>
#include <stdexcept>

#include "image_pass.h"

namespace {
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void Run() {
    using namespace rhythm;
    graph::Registry registry;
    auto image_node = registry.MakeNode(1, "texture.image");
    const assets::AssetId id{std::string(64, 'a')};
    image_node.properties_["asset"] = id;
    graph::Document document;
    document.id_ = "image.test";
    document.nodes_ = {image_node, registry.MakeNode(2, "output.texture")};
    document.edges_ = {{1, 1, 2, "source"}};
    document.output_ = 2;
    const auto plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
    auto resources = std::make_shared<assets::Images>();
    resources->images_.push_back({id, 4, 2, 1, 0, std::vector<std::uint8_t>(32, 255)});
    auto renderer = render::Renderer::CreateNull();
    runtime::detail::ImageUploads uploads;
    uploads.Retain(plan, *resources);
    auto draw = uploads.Draw(image_node, {100, 100}, *resources, renderer);
    Require(draw.vertices_[0].x_ == 0 && draw.vertices_[0].y_ == 25 &&
                    draw.vertices_[2].x_ == 100 && draw.vertices_[2].y_ == 75,
            "image aspect fit");
    const auto bytes = renderer.Stats().texture_bytes_;
    uploads.Draw(image_node, {200, 200}, *resources, renderer);
    Require(renderer.Stats().texture_bytes_ == bytes, "one asset upload across preview extents");
    image_node.properties_["image_fill"] = 1.0;
    draw = uploads.Draw(image_node, {100, 100}, *resources, renderer);
    Require(draw.vertices_[0].x_ == -50 && draw.vertices_[0].y_ == 0, "fill crops without stretch");
    image_node.properties_["image_fill"] = 0.0;
    resources->images_[0].clockwise_rotation_ = 90;
    draw = uploads.Draw(image_node, {100, 100}, *resources, renderer);
    Require(std::abs(draw.vertices_[0].x_ - 75) < 0.001 && std::abs(draw.vertices_[0].y_) < 0.001,
            "clockwise metadata rotation");
    resources->images_[0].clockwise_rotation_ = 0;
    uploads = {};
    runtime::Runtime runtime;
    runtime::FrameContext frame;
    frame.extent_ = {100, 100};
    frame.images_ = resources;
    renderer.BeginFrame();
    const auto first = runtime.Evaluate(plan, frame, renderer);
    renderer.EndFrame();
    const auto allocated = renderer.Stats().texture_bytes_;
    renderer.BeginFrame();
    const auto cached = runtime.Evaluate(plan, frame, renderer);
    Require(cached.final_ == first.final_ && cached.evaluated_ == 0 &&
                    renderer.Stats().passes_ == 0,
            "static image output cached");
    renderer.EndFrame();
    Require(renderer.Stats().texture_bytes_ == allocated, "stable image resources");
    frame.images_.reset();
    bool rejected = false;
    try {
        renderer.BeginFrame();
        runtime.Evaluate(plan, frame, renderer);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    renderer.EndFrame();
    Require(rejected, "missing source cannot reuse stale cached image");
    runtime.Reset();
    Require(renderer.Stats().texture_bytes_ == 0, "runtime releases image uploads and targets");
}
}  // namespace
int main() {
    try {
        Run();
        std::cout << "Images: fit/fill, orientation, shared upload, cache and resource release "
                     "passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
