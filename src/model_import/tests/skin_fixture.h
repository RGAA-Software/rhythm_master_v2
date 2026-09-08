#pragma once

#include "fixtures.h"

namespace rhythm::model_import::test {
inline ImageFixture SkinFixture() {
    ImageFixture fixture;
    fixture.json_ = R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":200}],
"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":6},
{"buffer":0,"byteOffset":44,"byteLength":12},{"buffer":0,"byteOffset":56,"byteLength":48},
{"buffer":0,"byteOffset":104,"byteLength":64},{"buffer":0,"byteOffset":168,"byteLength":8},
{"buffer":0,"byteOffset":176,"byteLength":24}],
"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},
{"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"},
{"bufferView":2,"componentType":5121,"count":3,"type":"VEC4"},
{"bufferView":3,"componentType":5126,"count":3,"type":"VEC4"},
{"bufferView":4,"componentType":5126,"count":1,"type":"MAT4"},
{"bufferView":5,"componentType":5126,"count":2,"type":"SCALAR"},
{"bufferView":6,"componentType":5126,"count":2,"type":"VEC3"}],
"meshes":[{"primitives":[{"attributes":{"POSITION":0,"JOINTS_0":2,"WEIGHTS_0":3},"indices":1}]}],
"nodes":[{"mesh":0,"skin":0,"translation":[1,0,0]},{"children":[0,2],"translation":[0,2,0]},
{"translation":[1,0,0]}],"scenes":[{"nodes":[1]}],"scene":0,
"skins":[{"joints":[2],"inverseBindMatrices":4}],
"animations":[{"name":"Joint sway","samplers":[{"input":5,"output":6}],
"channels":[{"sampler":0,"target":{"node":2,"path":"translation"}}]}]})";
    fixture.extra_.resize(12);
    for (int i = 0; i < 12; ++i) Word(fixture.extra_, std::bit_cast<std::uint32_t>(0.5f));
    for (const int value : {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -1, -2, 0, 1})
        Word(fixture.extra_, std::bit_cast<std::uint32_t>(static_cast<float>(value)));
    for (const int value : {0, 2, 1, 0, 0, 3, 0, 0})
        Word(fixture.extra_, std::bit_cast<std::uint32_t>(static_cast<float>(value)));
    return fixture;
}
}  // namespace rhythm::model_import::test
