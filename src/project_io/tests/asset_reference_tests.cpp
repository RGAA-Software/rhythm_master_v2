#include <iostream>
#include <stdexcept>

#include "graph.pb.h"
#include "rhythm/project/store.h"

namespace {
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void Run() {
    using namespace rhythm;
    graph::Document document;
    document.id_ = "asset-reference.test";
    document.output_ = 1;
    // An unknown future node remains authorable and round-trips typed properties.
    graph::Node node;
    node.id_ = 1;
    node.type_ = "test.asset.consumer";
    const assets::AssetId asset{std::string(64, 'a')};
    node.properties_["asset"] = asset;
    document.nodes_.push_back(node);
    const auto bytes = project::EncodeGraph(document);
    auto decoded = project::DecodeGraph(bytes);
    Require(std::get<assets::AssetId>(decoded.nodes_[0].properties_.at("asset")) == asset,
            "typed content reference survives authoring round trip");
    decoded.nodes_[0].properties_["asset"] = assets::AssetId{};
    Require(std::get<assets::AssetId>(project::DecodeGraph(project::EncodeGraph(decoded))
                                              .nodes_[0]
                                              .properties_.at("asset"))
                    .sha256_.empty(),
            "unassigned references remain editable and serializable");
    for (const auto& text :
         {std::string(64, 'G'), std::string("../outside"), std::string(65, 'a')}) {
        schema::GraphProject malformed;
        Require(malformed.ParseFromString(bytes), "fixture decodes");
        (*malformed.mutable_nodes(0)->mutable_properties())["asset"].set_asset_sha256(text);
        bool rejected = false;
        try {
            project::DecodeGraph(malformed.SerializeAsString());
        } catch (const std::exception&) {
            rejected = true;
        }
        Require(rejected, "invalid content addresses and oversized wire strings reject");
    }
    std::cout << "Asset references: typed round trip, unassigned values and wire limits passed\n";
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
