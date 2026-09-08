#include <cmath>
#include <iostream>
#include <stdexcept>

#include "rhythm/scene/model.h"

namespace {
void Require(bool condition) {
    if (!condition) throw std::runtime_error("scene skin reference mismatch");
}
template <typename Function>
void Reject(Function function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return;
    }
    throw std::runtime_error("invalid skin model accepted");
}
void Run() {
    using namespace rhythm::scene;
    auto model = Cube();
    model.nodes_[0].local_ = Compose({2, 0, 0}, {}, {1, 1, 1});
    model.nodes_[0].skin_ = 0;
    model.nodes_.push_back({2, 3, Compose({1, 0, 0}, {}, {1, 1, 1})});
    model.nodes_.push_back({3, {}, Compose({0, 2, 0}, {}, {1, 1, 1})});
    model.skins_.push_back({{2}, {Compose({-1, -2, 0}, {}, {1, 1, 1})}});
    model.meshes_[0].skin_.resize(model.meshes_[0].vertices_.size());
    Validate(model);
    auto worlds = WorldTransforms(model);
    auto palettes = SkinPalettes(model, worlds);
    Require(TransformPoint(Multiply(worlds.at(1).transform_, palettes.at(1)[0]), {1, 0, 0}) ==
            Vector3{1, 0, 0});
    AnimationPose pose{{2, {{3, 0, 0}}}};
    worlds = WorldTransforms(model, pose);
    palettes = SkinPalettes(model, worlds);
    Require(TransformPoint(Multiply(worlds.at(1).transform_, palettes.at(1)[0]), {1, 0, 0}) ==
            Vector3{3, 0, 0});
    const auto identity = Multiply(InverseAffine(model.nodes_[0].local_), model.nodes_[0].local_);
    Require(identity == Matrix{});
    GenerateTangents(model.meshes_[0]);
    Require(model.meshes_[0].skin_.size() == model.meshes_[0].vertices_.size());
    Validate(model);
    auto bad = model;
    bad.nodes_[0].skin_.reset();
    Reject([&] { Validate(bad); });
    bad = model;
    bad.skins_[0].joints_[0] = 99;
    Reject([&] { Validate(bad); });
    bad = model;
    bad.meshes_[0].skin_[0].joints_[3] = 1;
    Reject([&] { Validate(bad); });
    bad = model;
    bad.meshes_[0].skin_[0].weights_[0] = 0;
    Reject([&] { Validate(bad); });
    bad = model;
    bad.skins_[0].inverse_bind_.clear();
    Reject([&] { Validate(bad); });
    bad = model;
    bad.skins_[0].inverse_bind_[0].values_[0] = 0;
    Reject([&] { Validate(bad); });
    Reject([&] { SkinPalettes(model, {}); });
}
}  // namespace
int main() {
    try {
        Run();
        std::cout << "Scene skin: bind matrices, non-joint hierarchy, pose and tangent stream "
                     "passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
