#pragma once

#include "skin_fixture.h"

namespace rhythm::model_import::test {
inline ImageFixture MorphFixture() {
    auto fixture = SkinFixture();
    auto& json = fixture.json_;
    json = Replace(json, "\"byteLength\":200", "\"byteLength\":288");
    json = Replace(
            json, R"({"buffer":0,"byteOffset":176,"byteLength":24}])",
            R"({"buffer":0,"byteOffset":176,"byteLength":24},{"buffer":0,"byteOffset":200,"byteLength":36},{"buffer":0,"byteOffset":236,"byteLength":36},{"buffer":0,"byteOffset":272,"byteLength":16}])");
    json = Replace(
            json, R"({"bufferView":6,"componentType":5126,"count":2,"type":"VEC3"}])",
            R"({"bufferView":6,"componentType":5126,"count":2,"type":"VEC3"},{"bufferView":7,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":8,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":9,"componentType":5126,"count":4,"type":"SCALAR"}])");
    json = Replace(json, "\"indices\":1}",
                   R"("indices":1,"targets":[{"POSITION":7},{"POSITION":8}]})");
    json = Replace(json, R"("meshes":[{"primitives")",
                   R"("meshes":[{"weights":[0.1,0.2],"primitives")");
    json = Replace(json, R"("mesh":0,"skin":0)", R"("mesh":0,"skin":0,"weights":[0.3,0.4])");
    json = Replace(
            json, "\"animations\":[",
            R"("animations":[{"name":"Open shape","samplers":[{"input":5,"output":9}],"channels":[{"sampler":0,"target":{"node":0,"path":"weights"}}]},)");
    for (int target = 0; target < 2; ++target)
        for (int vertex = 0; vertex < 3; ++vertex)
            for (const float value : {target == 0 ? 0.5f : 0.0f, target == 1 ? 0.5f : 0.0f, 0.0f})
                Word(fixture.extra_, std::bit_cast<std::uint32_t>(value));
    for (const float value : {0.0f, 0.0f, 1.0f, 0.5f})
        Word(fixture.extra_, std::bit_cast<std::uint32_t>(value));
    return fixture;
}
}  // namespace rhythm::model_import::test
