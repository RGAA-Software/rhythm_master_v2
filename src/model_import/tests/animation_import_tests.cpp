#include <cmath>
#include <iostream>
#include <stdexcept>

#include "animation_fixture.h"
#include "rhythm/model_import/gltf.h"

namespace {
void Require(bool condition) {
    if (!condition) throw std::runtime_error("GLB animation import mismatch");
}
void Run() {
    using namespace rhythm;
    using namespace model_import::test;
    const auto fixture = AnimationFixture();
    auto bytes = Glb(fixture.json_, fixture.extra_);
    const auto model = model_import::ReadGlb(bytes);
    bytes.clear();
    Require(model.animations_.size() == 1 && model.animations_[0].Name() == "Sway" &&
            model.animations_[0].Duration() == 2 && model.rest_pose_.size() == 1);
    const auto pose = scene::Sample(model.animations_[0], model.rest_pose_, 1, false);
    const auto world = scene::WorldTransforms(model, pose);
    Require(scene::TransformPoint(world.at(1).transform_, {}) == scene::Vector3{2, 2, 0});
    const auto reject = [&](const std::string& json) {
        try {
            (void)model_import::ReadGlb(Glb(json, fixture.extra_));
        } catch (const std::invalid_argument&) {
            return;
        }
        throw std::runtime_error("invalid GLB animation accepted");
    };
    reject(Replace(fixture.json_, "\"path\":\"translation\"", "\"path\":\"rotation\""));
    reject(Replace(fixture.json_, "\"input\":2", "\"input\":0"));
    reject(Replace(fixture.json_, "\"node\":0", "\"node\":99"));
    reject(Replace(fixture.json_, "\"translation\":[1,0,0]",
                   "\"matrix\":[1,0,0,0,0,1,0,0,0,0,1,0,1,0,0,1]"));
    reject(Replace(fixture.json_, "LINEAR", "CUBICSPLINE"));
    auto invalid_time = fixture.extra_;
    for (std::size_t i = 4; i < 8; ++i) invalid_time[i] = 0;
    bool rejected = false;
    try {
        (void)model_import::ReadGlb(Glb(fixture.json_, invalid_time));
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    Require(rejected);
}
}  // namespace
int main() {
    try {
        Run();
        std::cout << "GLB owned animation, hierarchy sampling and malformed input passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
