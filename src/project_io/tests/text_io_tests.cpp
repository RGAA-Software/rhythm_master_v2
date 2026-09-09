#include <iostream>
#include <stdexcept>

#include "graph.pb.h"
#include "rhythm/graph/text.h"
#include "rhythm/project/package.h"
#include "rhythm/project/store.h"

namespace {
void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
template <class Function>
void Reject(Function function) {
    bool rejected = false;
    try {
        function();
    } catch (const std::exception&) {
        rejected = true;
    }
    Check(rejected, "invalid/downgraded text accepted");
}
}  // namespace
int main() {
    using namespace rhythm;
    try {
        graph::Registry registry;
        graph::Document document;
        document.id_ = "text-contract";
        document.nodes_ = {registry.MakeNode(1, "texture.text"),
                           registry.MakeNode(2, "output.texture")};
        document.edges_ = {{1, 1, 2, "source"}};
        document.output_ = 2;
        document.nodes_[0].properties_["text_content"] =
                std::string("棱镜星莲 / Rhythm\n音乐，2026！");
        const auto graph_bytes = project::EncodeGraph(document);
        schema::GraphProject graph_message;
        Check(graph_message.ParseFromString(graph_bytes) && graph_message.schema_version() == 8,
              "text schema");
        const auto decoded = project::DecodeGraph(graph_bytes);
        Check(decoded.nodes_[0].properties_ == document.nodes_[0].properties_,
              "text graph roundtrip");
        graph_message.set_schema_version(7);
        Reject([&] { project::DecodeGraph(graph_message.SerializeAsString()); });
        const auto compiled = graph::Compile(document, registry);
        Check(std::holds_alternative<graph::ExecutionPlan>(compiled), "text graph compile");
        const auto encoded = project::EncodeProgram(std::get<graph::ExecutionPlan>(compiled));
        schema::CompiledProgram program;
        Check(program.ParseFromString(encoded) && program.abi_version() == 6, "text program ABI");
        Check(project::DecodeProgram(encoded).instructions_[0].node_.properties_ ==
                      document.nodes_[0].properties_,
              "text program roundtrip");
        program.set_abi_version(5);
        Reject([&] { project::DecodeProgram(program.SerializeAsString()); });
        const auto key = graph::TextImageKey(document.nodes_[0]);
        auto changed = document.nodes_[0];
        changed.id_ = 500;
        Check(graph::TextImageKey(changed) == key, "node remapping invalidates text identity");
        changed.properties_["text_spacing"] = 1.200000001;
        Check(graph::TextImageKey(changed) != key, "layout identity rounds distinct values");
        changed.properties_["text_content"] = std::string("\xc0\xaf");
        Check(!registry.ValidateNode(changed).empty(), "invalid UTF-8 compiles");
        document.nodes_[0] = changed;
        Reject([&] { project::EncodeGraph(document); });
        Check(!graph::ValidText(std::string(4097, 'A')), "character budget");
        std::cout
                << "Text schema/ABI, UTF-8, exact cache identity and downgrade rejection passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
