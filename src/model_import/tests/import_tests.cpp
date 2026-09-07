#include <bit>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

#include "rhythm/model_import/gltf.h"

namespace {
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
const std::string kJson = R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":42}],
"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":6}],
"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},
{"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}],
"materials":[{"extensions":{"KHR_materials_unlit":{}},"pbrMetallicRoughness":{"baseColorFactor":[0.2,0.4,0.6,1]}}],
"meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1,"material":0}]}],
"nodes":[{"mesh":0,"translation":[1,0,0]},{"children":[0],"translation":[0,2,0]}],
"scenes":[{"nodes":[1]}],"scene":0})";
void Word(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8)
        bytes.push_back(static_cast<std::uint8_t>(value >> shift));
}
std::vector<std::uint8_t> Glb(std::string json = kJson) {
    while (json.size() % 4) json += ' ';
    std::vector<std::uint8_t> bin;
    for (const float value : {-1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f})
        Word(bin, std::bit_cast<std::uint32_t>(value));
    bin.insert(bin.end(), {0, 0, 1, 0, 2, 0, 0, 0});
    std::vector<std::uint8_t> bytes;
    Word(bytes, 0x46546c67);
    Word(bytes, 2);
    Word(bytes, static_cast<std::uint32_t>(28 + json.size() + bin.size()));
    Word(bytes, static_cast<std::uint32_t>(json.size()));
    Word(bytes, 0x4e4f534a);
    bytes.insert(bytes.end(), json.begin(), json.end());
    Word(bytes, static_cast<std::uint32_t>(bin.size()));
    Word(bytes, 0x004e4942);
    bytes.insert(bytes.end(), bin.begin(), bin.end());
    return bytes;
}
std::string Replace(std::string value, std::string_view from, std::string_view to) {
    const auto offset = value.find(from);
    Require(offset != std::string::npos, "fixture replacement exists");
    value.replace(offset, from.size(), to);
    return value;
}
void Reject(const std::vector<std::uint8_t>& bytes) {
    bool rejected = false;
    try {
        rhythm::model_import::ReadGlb(bytes);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    Require(rejected, "unsupported or malformed GLB rejects");
}
void Run() {
    using namespace rhythm;
    auto bytes = Glb();
    const auto model = model_import::ReadGlb(bytes);
    bytes.clear();
    Require(model.meshes_.size() == 1 && model.nodes_.size() == 2 && model.materials_.size() == 2,
            "GLB imports owned mesh, hierarchy and material values");
    const auto& mesh = model.meshes_[0];
    Require(mesh.indices_ == std::vector<std::uint32_t>{0, 1, 2} && mesh.vertices_[0].x_ == -1 &&
                    mesh.vertices_[0].normal_z_ == 1,
            "indices, positions and generated normals");
    Require(model.materials_[1].unlit_ &&
                    std::abs(model.materials_[1].base_color_.green_ - .4f) < 1e-6,
            "unlit factor preserved");
    const auto worlds = scene::WorldTransforms(model);
    Require(scene::TransformPoint(worlds.at(1).transform_, {}) == scene::Vector3{1, 2, 0},
            "GLB hierarchy transforms");
    auto broken = Glb();
    broken.pop_back();
    Reject(broken);
    Reject(Glb(Replace(kJson, "\"byteLength\":42",
                       "\"byteLength\":42,\"uri\":\"https://invalid.example/model.bin\"")));
    Reject(Glb(Replace(kJson, "\"count\":3", "\"count\":2147483647")));
    Reject(Glb(Replace(kJson, "\"byteOffset\":36", "\"byteOffset\":18446744073709551600")));
    Reject(Glb(Replace(kJson, "\"children\":[0]", "\"children\":[0,1]")));
    Reject(Glb(Replace(kJson, "\"POSITION\":0", "\"COLOR_0\":0")));
    Reject(Glb(Replace(kJson, "\"indices\":1", "\"indices\":1,\"mode\":1")));
    Reject(Glb(Replace(kJson,
                       "\"asset\":", "\"extensionsRequired\":[\"unknown_required\"],\"asset\":")));
    Reject(Glb("{\"extras\":" + std::string(65, '[') + "0" + std::string(65, ']') + "}"));
    std::stop_source stop;
    stop.request_stop();
    bool cancelled = false;
    try {
        model_import::ReadGlb(Glb(), stop.get_token());
    } catch (const std::runtime_error&) {
        cancelled = true;
    }
    Require(cancelled, "cancelled imports stop before parser allocation");
    std::cout << "GLB import: geometry/material/hierarchy ownership, bounds, URI/profile rejection "
                 "and cancellation passed\n";
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
