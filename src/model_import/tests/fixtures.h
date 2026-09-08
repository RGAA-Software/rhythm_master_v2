#pragma once

#include <bit>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace rhythm::model_import::test {
inline const std::string kJson = R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":42}],
"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":6}],
"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},
{"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}],
"materials":[{"extensions":{"KHR_materials_unlit":{}},"pbrMetallicRoughness":{"baseColorFactor":[0.2,0.4,0.6,1]}}],
"meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1,"material":0}]}],
"nodes":[{"mesh":0,"translation":[1,0,0]},{"children":[0],"translation":[0,2,0]}],
"scenes":[{"nodes":[1]}],"scene":0})";
inline void Word(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8)
        bytes.push_back(static_cast<std::uint8_t>(value >> shift));
}
inline std::vector<std::uint8_t> Glb(std::string json = kJson,
                                     std::vector<std::uint8_t> extra = {}) {
    while (json.size() % 4) json += ' ';
    std::vector<std::uint8_t> bin;
    for (const float value : {-1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f})
        Word(bin, std::bit_cast<std::uint32_t>(value));
    bin.insert(bin.end(), {0, 0, 1, 0, 2, 0, 0, 0});
    bin.insert(bin.end(), extra.begin(), extra.end());
    while (bin.size() % 4) bin.push_back(0);
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
inline std::string Replace(std::string value, std::string_view from, std::string_view to) {
    const auto offset = value.find(from);
    if (offset == std::string::npos) throw std::runtime_error("fixture replacement exists");
    value.replace(offset, from.size(), to);
    return value;
}
struct ImageFixture {
    std::string json_{};
    std::vector<std::uint8_t> extra_{};
};
inline ImageFixture EmbeddedFixture() {
    const std::vector<std::uint8_t> png{
            137, 80, 78, 71, 13, 10,  26,  10,  0,   0,   0,   13, 73,  72,  68, 82,  0,
            0,   0,  2,  0,  0,  0,   2,   8,   6,   0,   0,   0,  114, 182, 13, 36,  0,
            0,   0,  27, 73, 68, 65,  84,  120, 156, 99,  56,  17, 160, 193, 32, 178, 224,
            67,  3,  67, 74, 69, 207, 255, 59,  39,  182, 252, 7,  0,   67,  96, 9,   35,
            234, 97, 45, 93, 0,  0,   0,   0,   73,  69,  78,  68, 174, 66,  96, 130};
    std::vector<std::uint8_t> extra;
    for (const float value : {0.0f, 0.0f, 1.0f, 0.0f, 0.5f, 1.0f})
        Word(extra, std::bit_cast<std::uint32_t>(value));
    extra.insert(extra.end(), png.begin(), png.end());
    auto json = Replace(kJson, "\"byteLength\":42",
                        "\"byteLength\":" + std::to_string(68 + png.size()));
    json = Replace(json, "\"byteLength\":6}]",
                   "\"byteLength\":6},{\"buffer\":0,\"byteOffset\":44,\"byteLength\":24},{"
                   "\"buffer\":0,\"byteOffset\":68,\"byteLength\":" +
                           std::to_string(png.size()) + "}]");
    json = Replace(json, "\"type\":\"SCALAR\"}]",
                   "\"type\":\"SCALAR\"},{\"bufferView\":2,\"componentType\":5126,\"count\":3,"
                   "\"type\":\"VEC2\"}]");
    json = Replace(json, "\"POSITION\":0", "\"POSITION\":0,\"TEXCOORD_0\":2");
    json = Replace(
            json, "\"materials\":",
            R"("images":[{"bufferView":3,"mimeType":"image/png"}],"textures":[{"source":0}],"materials":)");
    json = Replace(
            json, "\"pbrMetallicRoughness\":{",
            R"("normalTexture":{"index":0,"scale":0.3},"emissiveTexture":{"index":0},"occlusionTexture":{"index":0,"strength":0.5},"pbrMetallicRoughness":{"baseColorTexture":{"index":0},"metallicRoughnessTexture":{"index":0},)");
    return {std::move(json), std::move(extra)};
}
}  // namespace rhythm::model_import::test
