#include <iostream>
#include <stdexcept>

#include "rhythm/editor/commands.h"
#include "rhythm/player/prepared_package.h"

namespace {
void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
}  // namespace
int main() {
    using namespace rhythm;
    try {
        graph::Registry registry;
        editor::Snapshot initial;
        initial.document_.id_ = "time.sections";
        initial.document_.nodes_ = {registry.MakeNode(1, "texture.gradient"),
                                    registry.MakeNode(2, "output.texture")};
        initial.document_.edges_ = {{1, 1, 2, "source"}};
        initial.document_.output_ = 2;
        editor::History history(initial);
        const auto section = history.ReserveNodeId(), clock = history.ReserveNodeId();
        auto added = editor::AddTimeSection(history.Current(), registry, 8, section, clock);
        Check(std::holds_alternative<editor::Snapshot>(added), "add section and clock");
        Check(history.Apply(std::get<editor::Snapshot>(added),
                            history.Current().document_.revision_),
              "single history command");
        Check(history.Current().document_.nodes_.size() == 4 && history.Undo() &&
                      history.Current().document_.nodes_ == initial.document_.nodes_,
              "one undo removes section and generated clock");
        Check(history.Redo(), "redo section");
        const auto next_id = history.ReserveNodeId();
        Check(next_id > clock, "reserved IDs survive undo");
        const auto second = editor::AddTimeSection(history.Current(), registry, 16, next_id,
                                                   history.ReserveNodeId());
        Check(std::get<editor::Snapshot>(second).document_.nodes_.size() == 5, "reuse root clock");
        const auto connected = editor::Connect(history.Current(), registry, section, 1, "amount");
        const auto& work = std::get<editor::Snapshot>(connected);
        const auto bytes = project::EncodePackage(work.document_, "Timed visual");
        const auto package = project::DecodePackage(bytes);
        bool found = false;
        for (const auto& instruction : package.program_.instructions_)
            if (instruction.node_.id_ == section) {
                Check(instruction.operation_ == graph::Operation::kTimeEnvelope &&
                              graph::Scalar(instruction.node_, "clip_start", -1) == 8,
                      "published section identity, operation and parameters survive");
                found = true;
            }
        Check(found && player::PreparedPackage(bytes).SupportsAnalyticSeek(),
              "section is reachable and analytically seekable");
        Check(std::holds_alternative<graph::Diagnostic>(
                      editor::AddTimeSection(initial, registry, -1, 3, 4)),
              "invalid interval rejected");
        Check(std::holds_alternative<graph::Diagnostic>(
                      editor::AddTimeSection(initial, registry, 1, 1, 4)),
              "duplicate ID rejected without modifying source");
        std::cout << "time section add/connect/undo/ID allocation and portable package contracts "
                     "pass\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
