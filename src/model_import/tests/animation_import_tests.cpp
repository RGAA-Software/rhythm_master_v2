#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

#include "animation_fixture.h"
#include "rhythm/model_import/gltf.h"
#include "skin_fixture.h"

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
    const auto skinned = SkinFixture();
    const auto skin_model = model_import::ReadGlb(Glb(skinned.json_, skinned.extra_));
    Require(skin_model.skins_.size() == 1 && skin_model.meshes_[0].skin_.size() == 3 &&
            skin_model.meshes_[0].skin_[0].weights_[0] == 0.25f);
    const auto joint_pose =
            scene::Sample(skin_model.animations_[0], skin_model.rest_pose_, 1, false);
    const auto joint_worlds = scene::WorldTransforms(skin_model, joint_pose);
    const auto palettes = scene::SkinPalettes(skin_model, joint_worlds);
    Require(scene::TransformPoint(scene::Multiply(joint_worlds.at(1).transform_, palettes.at(1)[0]),
                                  {}) == scene::Vector3{1, 0, 0});
    const auto skin_reject = [&](const std::string& json, const std::vector<std::uint8_t>& bin) {
        try {
            (void)model_import::ReadGlb(Glb(json, bin));
        } catch (const std::invalid_argument&) {
            return;
        }
        throw std::runtime_error("invalid skin GLB accepted");
    };
    skin_reject(Replace(skinned.json_, "JOINTS_0", "JOINTS_1"), skinned.extra_);
    skin_reject(Replace(skinned.json_, "\"joints\":[2]", "\"joints\":[99]"), skinned.extra_);
    skin_reject(Replace(skinned.json_, "\"skin\":0,", ""), skinned.extra_);
    auto bad_skin = skinned.extra_;
    bad_skin[0] = 1;
    skin_reject(skinned.json_, bad_skin);
    bad_skin = skinned.extra_;
    std::fill(bad_skin.begin() + 12, bad_skin.begin() + 28, std::uint8_t{0});
    skin_reject(skinned.json_, bad_skin);
    const auto no_bind = model_import::ReadGlb(
            Glb(Replace(skinned.json_, ",\"inverseBindMatrices\":4", ""), skinned.extra_));
    Require(no_bind.skins_[0].inverse_bind_[0] == scene::Matrix{});
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
