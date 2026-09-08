#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "rhythm/scene/model.h"

namespace {
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void Run() {
    using namespace rhythm::scene;
    for (auto model : {Cube(), Sphere(), Torus()}) {
        for (auto& mesh : model.meshes_) GenerateTangents(mesh);
        Validate(model);
        Require(model.meshes_[0].has_tangents_, "primitive has valid orthogonal tangent frames");
    }
    Mesh mesh;
    mesh.vertices_ = {{0, 0, 0, 0, 0, 1, 0, 0},
                      {1, 0, 0, 0, 0, 1, 1, 0},
                      {0, 1, 0, 0, 0, 1, 0, 1},
                      {-1, 0, 0, 0, 0, 1, 1, 0}};
    mesh.indices_ = {0, 1, 2, 0, 2, 3};
    GenerateTangents(mesh);
    Require(mesh.vertices_.size() == 6, "mirrored UV splits both shared corners");
    const auto& a = mesh.vertices_[mesh.indices_[0]].tangent_;
    const auto& b = mesh.vertices_[mesh.indices_[3]].tangent_;
    Require(a[0] > 0.99f && b[0] < -0.99f && a[3] == -b[3],
            "mirrored tangent and handedness reconstruct the correct bitangent");
    const auto indices = mesh.indices_;
    mesh.vertices_[0].u_ = std::numeric_limits<float>::quiet_NaN();
    bool rejected = false;
    try {
        GenerateTangents(mesh);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    Require(rejected && mesh.indices_ == indices && mesh.vertices_.size() == 6,
            "invalid input does not partially replace mesh topology");
}
}  // namespace
int main() {
    try {
        Run();
        std::cout << "MikkTSpace primitives, mirrored UV seams and atomic rejection passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
