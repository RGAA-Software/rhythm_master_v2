#pragma once

#include "fixtures.h"

namespace rhythm::model_import::test {
inline ImageFixture AnimationFixture() {
    ImageFixture fixture;
    fixture.json_ = Replace(kJson, "\"byteLength\":42", "\"byteLength\":76");
    fixture.json_ = Replace(
            fixture.json_, "\"bufferViews\":[",
            R"("bufferViews":[{"buffer":0,"byteOffset":44,"byteLength":8},{"buffer":0,"byteOffset":52,"byteLength":24},)");
    // Original geometry views shifted by two; accessor indices stay unchanged.
    fixture.json_ = Replace(fixture.json_, "\"bufferView\":0", "\"bufferView\":2");
    fixture.json_ = Replace(fixture.json_, "\"bufferView\":1", "\"bufferView\":3");
    fixture.json_ = Replace(
            fixture.json_, "\"materials\":[",
            R"("animations":[{"name":"Sway","samplers":[{"input":2,"output":3,"interpolation":"LINEAR"}],"channels":[{"sampler":0,"target":{"node":0,"path":"translation"}}]}],"materials":[)");
    fixture.json_ = Replace(
            fixture.json_, "\"type\":\"SCALAR\"}]",
            R"("type":"SCALAR"},{"bufferView":0,"componentType":5126,"count":2,"type":"SCALAR"},{"bufferView":1,"componentType":5126,"count":2,"type":"VEC3"}])");
    for (const float value : {0.0f, 2.0f, 1.0f, 0.0f, 0.0f, 3.0f, 0.0f, 0.0f})
        Word(fixture.extra_, std::bit_cast<std::uint32_t>(value));
    return fixture;
}
}  // namespace rhythm::model_import::test
