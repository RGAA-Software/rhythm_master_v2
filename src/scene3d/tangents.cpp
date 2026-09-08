#include <mikktspace.h>

#include <bit>
#include <cmath>
#include <functional>
#include <map>
#include <stdexcept>
#include <utility>

#include "rhythm/scene/model.h"

namespace rhythm::scene {
namespace {
using Tangent = std::array<float, 4>;
using Work = std::pair<std::reference_wrapper<const Mesh>, std::vector<Tangent>>;
// MikkTSpace borrows this stack context and array destinations only during
// genTangSpaceDefault. No callback or pointer escapes this synchronous adapter.
Work& Data(const SMikkTSpaceContext* context) {
    if (!context || !context->m_pUserData) std::terminate();
    return *static_cast<Work*>(context->m_pUserData);
}
const Vertex& Corner(const SMikkTSpaceContext* context, int face, int corner) {
    const auto& mesh = Data(context).first.get();
    return mesh.vertices_[mesh.indices_[std::size_t(face) * 3 + corner]];
}
int Faces(const SMikkTSpaceContext* context) {
    return static_cast<int>(Data(context).first.get().indices_.size() / 3);
}
int Corners(const SMikkTSpaceContext*, int) { return 3; }
void Position(const SMikkTSpaceContext* context, float out[], int face, int corner) {
    const auto& v = Corner(context, face, corner);
    out[0] = v.x_;
    out[1] = v.y_;
    out[2] = v.z_;
}
void Normal(const SMikkTSpaceContext* context, float out[], int face, int corner) {
    const auto& v = Corner(context, face, corner);
    out[0] = v.normal_x_;
    out[1] = v.normal_y_;
    out[2] = v.normal_z_;
}
void Uv(const SMikkTSpaceContext* context, float out[], int face, int corner) {
    const auto& v = Corner(context, face, corner);
    out[0] = v.u_;
    out[1] = v.v_;
}
void Store(const SMikkTSpaceContext* context, const float tangent[], float sign, int face,
           int corner) {
    Data(context).second[std::size_t(face) * 3 + corner] = {tangent[0], tangent[1], tangent[2],
                                                            sign};
}
void Require(bool condition) {
    if (!condition) throw std::invalid_argument("scene.tangents");
}
}  // namespace
void GenerateTangents(Mesh& mesh) {
    Require(!mesh.vertices_.empty() && mesh.vertices_.size() <= 250000 && !mesh.indices_.empty() &&
            mesh.indices_.size() <= 750000 && mesh.indices_.size() % 3 == 0);
    for (const auto index : mesh.indices_) Require(index < mesh.vertices_.size());
    for (const auto& v : mesh.vertices_) {
        for (const auto value : {v.x_, v.y_, v.z_, v.u_, v.v_})
            Require(std::isfinite(value) && std::abs(value) <= 1e6f);
        const auto length = std::hypot(v.normal_x_, v.normal_y_, v.normal_z_);
        Require(std::isfinite(length) && length > 0.99f && length < 1.01f);
    }
    Work work{std::cref(mesh), std::vector<Tangent>(mesh.indices_.size())};
    SMikkTSpaceInterface interface {};
    interface.m_getNumFaces = Faces;
    interface.m_getNumVerticesOfFace = Corners;
    interface.m_getPosition = Position;
    interface.m_getNormal = Normal;
    interface.m_getTexCoord = Uv;
    interface.m_setTSpaceBasic = Store;
    SMikkTSpaceContext context{&interface, &work};
    Require(genTangSpaceDefault(&context) != 0);
    Mesh result;
    result.material_ = mesh.material_;
    result.has_tangents_ = true;
    result.indices_.reserve(mesh.indices_.size());
    // Never overwrite a shared vertex with the last face's frame. Exact output
    // frames weld only when their original vertex identity also matches.
    std::map<std::array<std::uint32_t, 5>, std::uint32_t> lookup;
    for (std::size_t i = 0; i < mesh.indices_.size(); ++i) {
        const auto source = mesh.indices_[i];
        auto tangent = work.second[i];
        const auto& v = mesh.vertices_[source];
        const auto length = std::hypot(tangent[0], tangent[1], tangent[2]);
        Require(std::isfinite(length) && length > 0.99f && length < 1.01f &&
                std::abs(tangent[3]) == 1 &&
                std::abs(tangent[0] * v.normal_x_ + tangent[1] * v.normal_y_ +
                         tangent[2] * v.normal_z_) < 0.01f);
        std::array<std::uint32_t, 5> key{source};
        for (std::size_t component = 0; component < 4; ++component) {
            if (tangent[component] == 0) tangent[component] = 0;  // Canonicalize negative zero.
            key[component + 1] = std::bit_cast<std::uint32_t>(tangent[component]);
        }
        const auto [entry, inserted] = lookup.emplace(key, std::uint32_t(result.vertices_.size()));
        if (inserted) {
            Require(result.vertices_.size() < 250000);
            auto vertex = v;
            vertex.tangent_ = tangent;
            result.vertices_.push_back(vertex);
        }
        result.indices_.push_back(entry->second);
    }
    mesh = std::move(result);
}
}  // namespace rhythm::scene
