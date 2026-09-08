#include <bit>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

#include "fixtures.h"
#include "rhythm/model_import/gltf.h"

namespace {
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
using namespace rhythm::model_import::test;
void Reject(const std::vector<std::uint8_t>& bytes) {
    bool rejected = false;
    try {
        rhythm::model_import::ReadGlb(bytes);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    Require(rejected, "unsupported or malformed GLB rejects");
}
#if defined(RHYTHM_TEST_IMAGE_DECODER)
void EmbeddedImages() {
    using namespace rhythm;
    auto fixture = EmbeddedFixture();
    auto& json = fixture.json_;
    auto& extra = fixture.extra_;
    const auto model = model_import::ReadGlb(Glb(json, extra));
    Require(model.images_.size() == 2 && model.images_[0].width_ == 2 &&
                    model.images_[0].height_ == 2,
            "embedded PNG decoded by FFmpeg and ORM prepared");
    Require(model.images_[0].rgba_[0] == 200 && model.images_[0].rgba_[3] == 255 &&
                    model.images_[0].rgba_[4] == 20 && model.images_[0].rgba_[7] == 255,
            "opaque glTF ignores texture alpha without discarding RGB");
    const auto& material = model.materials_[1];
    Require(material.textures_.images_[0] == 0u && material.textures_.images_[1] == 0u &&
                    material.textures_.images_[2] == 1u && material.textures_.images_[3] == 0u &&
                    std::abs(material.textures_.normal_scale_ - 0.3f) < 1e-6,
            "all material bindings and normal scale retained");
    Require(model.images_[1].rgba_[0] == 228 && model.images_[1].rgba_[1] == 80 &&
                    model.images_[1].rgba_[2] == 40,
            "occlusion strength combines with original roughness/metal channels");
    Require(model.meshes_[0].vertices_[2].u_ == 0.5f && model.meshes_[0].vertices_[2].v_ == 1,
            "glTF UV0 stays top-left");
    Reject(Glb(Replace(json, "\"bufferView\":3", "\"uri\":\"texture.png\""), extra));
    Reject(Glb(Replace(json, "image/png", "image/gif"), extra));
    Reject(Glb(Replace(json, "\"scale\":0.3", "\"scale\":0.3,\"texCoord\":1"), extra));
    Reject(Glb(Replace(json, "\"TEXCOORD_0\":2", "\"TEXCOORD_1\":2"), extra));
    auto sampler = Replace(json, "\"textures\":[{\"source\":0}]",
                           R"("samplers":[{"wrapS":33071}],"textures":[{"source":0,"sampler":0}])");
    Reject(Glb(sampler, extra));
    std::cout << "GLB images: FFmpeg RGBA, opaque RGB, bindings, ORM, UV and unsupported profiles "
                 "passed\n";
}
#endif
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
#if defined(RHYTHM_TEST_IMAGE_DECODER)
        EmbeddedImages();
#endif
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
