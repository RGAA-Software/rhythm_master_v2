#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "rhythm/scene/path.h"

namespace {
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
template <typename Function>
void Reject(Function action) {
    bool rejected = false;
    try {
        action();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    Require(rejected, "invalid path must reject before publishing geometry");
}
void CheckTube(const rhythm::scene::Path& path, double radius, std::uint32_t sides) {
    using namespace rhythm::scene;
    auto model = Tube(path, radius, sides);
    Validate(model);
    auto& mesh = model.meshes_[0];
    const auto rings = path.points_.size() + (path.closed_ ? 1 : 0);
    const auto expected_vertices = rings * (sides + 1) + (path.closed_ ? 0 : 2 * (sides + 2));
    Require(mesh.vertices_.size() == expected_vertices &&
                    mesh.indices_.size() ==
                            (rings - 1) * sides * 6 + (path.closed_ ? 0 : sides * 6),
            "tube topology and cap counts");
    for (std::size_t ring = 0; ring < rings; ++ring) {
        const auto& a = mesh.vertices_[ring * (sides + 1)];
        const auto& b = mesh.vertices_[ring * (sides + 1) + sides];
        Require(a.x_ == b.x_ && a.y_ == b.y_ && a.z_ == b.z_ && a.u_ == 0 && b.u_ == 1,
                "radial UV seam is geometrically exact");
        const auto center = path.points_[ring % path.points_.size()];
        Require(std::abs(std::hypot(a.x_ - center.x_, a.y_ - center.y_, a.z_ - center.z_) -
                         radius) < 1e-5,
                "tube follows the requested radius");
    }
    if (path.closed_) {
        for (std::uint32_t side = 0; side <= sides; ++side) {
            const auto& a = mesh.vertices_[side];
            const auto& b = mesh.vertices_[(rings - 1) * (sides + 1) + side];
            Require(a.x_ == b.x_ && a.y_ == b.y_ && a.z_ == b.z_ && a.normal_x_ == b.normal_x_ &&
                            a.normal_y_ == b.normal_y_ && a.normal_z_ == b.normal_z_,
                    "closed transport frame and geometry join without a seam");
        }
    }
    for (std::size_t i = 0; i < mesh.indices_.size(); i += 3) {
        const auto& a = mesh.vertices_[mesh.indices_[i]];
        const auto& b = mesh.vertices_[mesh.indices_[i + 1]];
        const auto& c = mesh.vertices_[mesh.indices_[i + 2]];
        const auto normal = Cross({b.x_ - a.x_, b.y_ - a.y_, b.z_ - a.z_},
                                  {c.x_ - a.x_, c.y_ - a.y_, c.z_ - a.z_});
        Require(Dot(normal, {a.normal_x_, a.normal_y_, a.normal_z_}) > 0,
                "caps and sides have outward CCW triangles");
    }
    GenerateTangents(mesh);
    Validate(model);
}
void Run() {
    using namespace rhythm::scene;
    const Path straight{{{0, 0, -1}, {0, 0, 1}}};
    const auto smooth = Resample(straight, 17);
    Require(smooth.points_.front() == straight.points_.front() &&
                    smooth.points_.back() == straight.points_.back(),
            "open spline preserves endpoints");
    for (std::size_t i = 1; i < smooth.points_.size(); ++i)
        Require(smooth.points_[i].z_ > smooth.points_[i - 1].z_, "straight spline stays monotonic");
    CheckTube(straight, 0.1, 8);
    CheckTube(Helix(128, 1, 3, 2, 25), 0.05, 12);
    CheckTube(Resample(Helix(16, 1, 0, 1, 0, true), 128), 0.05, 12);
    Require(Tube(Path{}, 0.1).meshes_.empty(), "empty point input produces empty geometry");
    Reject([&] { Validate(Path{{{0, 0, 0}}}); });
    Reject([&] { Resample(straight, 1025); });
    Reject([&] { Tube(straight, 0); });
    Reject([&] { Tube(straight, 1, 33); });
    Reject([&] { Helix(16, 1, 0, 0, 0); });
    auto invalid = straight;
    invalid.points_[1] = invalid.points_[0];
    Reject([&] { Tube(invalid, 0.1); });
    invalid.points_[1].x_ = std::numeric_limits<double>::infinity();
    Reject([&] { Resample(invalid, 32); });
}
}  // namespace
int main() {
    try {
        Run();
        std::cout << "Paths: Catmull-Rom endpoints, helix, tube frames/seams/caps, winding, "
                     "tangents and bounds passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
