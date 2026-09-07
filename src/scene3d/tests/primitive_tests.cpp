#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "rhythm/scene/model.h"

namespace {
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void Check(float radius, float height, std::uint32_t segments, std::uint32_t rings) {
    using namespace rhythm::scene;
    const auto model = Sphere(radius, height, segments, rings);
    Validate(model);
    const auto& mesh = model.meshes_.at(0);
    for (const auto& vertex : mesh.vertices_) {
        const Vector3 position{vertex.x_, vertex.y_, vertex.z_};
        const Vector3 normal{vertex.normal_x_, vertex.normal_y_, vertex.normal_z_};
        const double radial = radius, vertical = height * 0.5;
        const auto surface =
                (position.x_ * position.x_ + position.z_ * position.z_) / (radial * radial) +
                position.y_ * position.y_ / (vertical * vertical);
        Require(std::abs(surface - 1) < 1e-5, "vertices lie on analytic ellipsoid");
        const auto gradient =
                Normalize({position.x_ / (radial * radial), position.y_ / (vertical * vertical),
                           position.z_ / (radial * radial)});
        Require(Dot(gradient, normal) > 0.99999, "normal matches ellipsoid gradient");
        Require(vertex.u_ >= 0 && vertex.u_ <= 1 && vertex.v_ >= 0 && vertex.v_ <= 1,
                "UV coordinates remain in unit range");
    }
    for (std::size_t row = 0; row <= rings + 1; ++row) {
        const auto& a = mesh.vertices_.at(row * (segments + 1));
        const auto& b = mesh.vertices_.at(row * (segments + 1) + segments);
        Require(a.x_ == b.x_ && a.y_ == b.y_ && a.z_ == b.z_ && a.u_ == 0 && b.u_ == 1,
                "UV seam closes exactly without a geometry gap");
    }
    std::size_t nondegenerate = 0;
    for (std::size_t i = 0; i < mesh.indices_.size(); i += 3) {
        const auto& a = mesh.vertices_.at(mesh.indices_[i]);
        const auto& b = mesh.vertices_.at(mesh.indices_[i + 1]);
        const auto& c = mesh.vertices_.at(mesh.indices_[i + 2]);
        const auto cross = Cross({double(b.x_) - a.x_, double(b.y_) - a.y_, double(b.z_) - a.z_},
                                 {double(c.x_) - a.x_, double(c.y_) - a.y_, double(c.z_) - a.z_});
        if (Dot(cross, cross) < 1e-20) continue;
        ++nondegenerate;
        Require(Dot(cross, {a.normal_x_, a.normal_y_, a.normal_z_}) > 0,
                "nondegenerate triangle front faces point outward");
    }
    Require(nondegenerate > 0, "primitive contains renderable faces");
}
void Run() {
    Check(0.5f, 1, 32, 16);
    Check(2, 1, 24, 12);
    Check(0.25f, 3, 16, 8);
    Check(1, 2, 3, 1);
    Check(1, 2, 256, 128);
    using rhythm::scene::Sphere;
    for (const auto radius : {0.0f, -1.0f, std::numeric_limits<float>::infinity(),
                              std::numeric_limits<float>::quiet_NaN()}) {
        bool rejected = false;
        try {
            Sphere(radius);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        Require(rejected, "invalid dimensions reject before allocation");
    }
    for (const auto segments : {0U, 2U, 257U, std::numeric_limits<unsigned>::max()}) {
        bool rejected = false;
        try {
            Sphere(1, 2, segments);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        Require(rejected, "invalid tessellation rejects before count arithmetic");
    }
    std::cout << "Godot sphere adaptation: analytic surface, normals, seams, winding and limits "
                 "passed\n";
}
}  // namespace
int main() {
    try {
        Run();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
