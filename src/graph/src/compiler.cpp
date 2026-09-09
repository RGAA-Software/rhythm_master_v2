#include "rhythm/graph/compiler.h"

#include <algorithm>
#include <cmath>
#include <queue>
#include <set>
#include <unordered_map>

#include "rhythm/graph/bindings.h"
#include "rhythm/graph/components.h"
#include "rhythm/graph/controls.h"

namespace rhythm::graph {
CompileResult Compile(const Document& document, const Registry& registry,
                      std::span<const NodeId> viewers) {
    if (!document.components_.empty()) {
        auto expanded = ExpandComponents(document, registry);
        if (std::holds_alternative<std::vector<Diagnostic>>(expanded))
            return std::get<std::vector<Diagnostic>>(std::move(expanded));
        return Compile(std::get<Document>(expanded), registry, viewers);
    }
    std::vector<Diagnostic> diagnostics;
    const auto fail = [&](std::string code, NodeId node = 0, std::string field = {}) {
        diagnostics.push_back({std::move(code), node, std::move(field)});
    };
    if (!ValidCanvas(document.canvas_)) {
        fail("graph.canvas");
        return diagnostics;
    }
    if (document.beat_grid_ && !parameters::ValidBeatSettings(*document.beat_grid_)) {
        fail("graph.beat_settings");
        return diagnostics;
    }
    if (document.nodes_.size() > 10000 || document.edges_.size() > 40000 || viewers.size() > 16) {
        fail("graph.limit");
        return diagnostics;
    }
    auto resolved = ResolveEdges(document);
    if (std::holds_alternative<std::vector<Diagnostic>>(resolved)) {
        return std::get<std::vector<Diagnostic>>(std::move(resolved));
    }
    std::unordered_map<NodeId, std::size_t> index;
    std::vector<OperatorDescriptor> descriptors;
    for (std::size_t i = 0; i < document.nodes_.size(); ++i) {
        const auto& node = document.nodes_[i];
        if (!node.id_ || !index.emplace(node.id_, i).second) fail("graph.duplicate_node", node.id_);
        const auto descriptor = registry.Find(node.type_);
        if (!descriptor || node.version_ != 1) {
            fail("graph.unsupported_node", node.id_);
            descriptors.emplace_back();
            continue;
        }
        descriptors.push_back(*descriptor);
        const auto properties = registry.ValidateNode(node);
        diagnostics.insert(diagnostics.end(), properties.begin(), properties.end());
    }
    if (!diagnostics.empty()) return diagnostics;
    parameters::ControlBank controls;
    try {
        controls = DescribeControls(document);
        if (!document.control_cues_.empty())
            (void)parameters::ControlSequence(controls, document.control_cues_);
    } catch (const std::exception&) {
        fail("graph.controls");
        return diagnostics;
    }
    std::vector<std::vector<std::optional<std::size_t>>> inputs(descriptors.size());
    for (std::size_t i = 0; i < inputs.size(); ++i) inputs[i].resize(descriptors[i].inputs_.size());
    std::set<std::uint64_t> edge_ids;
    for (const auto& edge : std::get<std::vector<Edge>>(resolved)) {
        if (!edge.id_ || !edge_ids.insert(edge.id_).second) fail("graph.duplicate_edge", edge.to_);
        if (!index.contains(edge.from_) || !index.contains(edge.to_)) {
            fail("graph.missing_node", edge.to_);
            continue;
        }
        const auto from = index.at(edge.from_);
        const auto to = index.at(edge.to_);
        const auto& ports = descriptors[to].inputs_;
        const auto port = std::find_if(ports.begin(), ports.end(),
                                       [&](const auto& item) { return item.key_ == edge.input_; });
        if (port == ports.end()) {
            fail("graph.missing_port", edge.to_, edge.input_);
            continue;
        }
        const auto port_index = static_cast<std::size_t>(port - ports.begin());
        if (inputs[to][port_index]) fail("graph.input_connected", edge.to_, edge.input_);
        if (port->type_ != descriptors[from].output_)
            fail("graph.port_type", edge.to_, edge.input_);
        inputs[to][port_index] = from;
    }
    if (!index.contains(document.output_))
        fail("graph.missing_output");
    else if (descriptors[index.at(document.output_)].operation_ != Operation::kOutput)
        fail("graph.output_type");
    for (const auto viewer : viewers)
        if (!index.contains(viewer)) fail("graph.missing_viewer", viewer);
    if (!diagnostics.empty()) return diagnostics;

    // Iterative demand traversal remains bounded even for large graphs and feedback cycles.
    std::vector<bool> demanded(inputs.size(), false);
    std::vector<std::size_t> stack{index.at(document.output_)};
    for (const auto viewer : viewers) stack.push_back(index.at(viewer));
    while (!stack.empty()) {
        const auto node = stack.back();
        stack.pop_back();
        if (demanded[node]) continue;
        demanded[node] = true;
        // A Cue event source depends on the saved Cue/snapshot bank even when
        // those macro values have no continuous connection to the final image.
        if (descriptors[node].operation_ == Operation::kEventCue)
            for (std::size_t control = 0; control < descriptors.size(); ++control)
                if (descriptors[control].operation_ == Operation::kControlScalar)
                    stack.push_back(control);
        for (std::size_t port = 0; port < inputs[node].size(); ++port) {
            if (inputs[node][port])
                stack.push_back(*inputs[node][port]);
            else if (descriptors[node].inputs_[port].required_)
                fail("graph.required_input", document.nodes_[node].id_,
                     descriptors[node].inputs_[port].key_);
        }
    }
    if (!diagnostics.empty()) return diagnostics;

    for (std::size_t node = 0; node < descriptors.size(); ++node)
        if (demanded[node] && descriptors[node].operation_ == Operation::kEventBeat &&
            !document.beat_grid_)
            fail("event.beat_grid_required", document.nodes_[node].id_);
    if (!diagnostics.empty()) return diagnostics;

    // Validate ordinary cycles across the whole document; culling is not a way to hide invalid
    // cycles.
    std::vector<std::vector<std::size_t>> dependents(inputs.size());
    std::vector<std::size_t> degrees(inputs.size(), 0);
    for (std::size_t node = 0; node < inputs.size(); ++node) {
        if (descriptors[node].operation_ == Operation::kFeedback) continue;
        for (const auto input : inputs[node])
            if (input) {
                dependents[*input].push_back(node);
                ++degrees[node];
            }
    }
    std::priority_queue<std::size_t, std::vector<std::size_t>, std::greater<>> ready;
    for (std::size_t node = 0; node < degrees.size(); ++node)
        if (!degrees[node]) ready.push(node);
    ExecutionPlan plan;
    plan.document_id_ = document.id_;
    plan.revision_ = document.revision_;
    plan.canvas_ = document.canvas_;
    plan.beat_grid_ = document.beat_grid_;
    std::vector<std::size_t> remap(inputs.size());
    std::size_t visited = 0;
    while (!ready.empty()) {
        const auto node = ready.top();
        ready.pop();
        ++visited;
        if (demanded[node]) {
            remap[node] = plan.instructions_.size();
            plan.instructions_.push_back(
                    {document.nodes_[node], descriptors[node].operation_, inputs[node]});
        }
        for (const auto child : dependents[node])
            if (--degrees[child] == 0) ready.push(child);
    }
    if (visited != inputs.size()) {
        fail("graph.cycle");
        return diagnostics;
    }
    for (auto& instruction : plan.instructions_)
        for (auto& input : instruction.inputs_)
            if (input) input = remap[*input];
    plan.output_ = remap[index.at(document.output_)];
    if (const auto budget = ValidatePointBudget(plan)) return std::vector<Diagnostic>{*budget};
    if (const auto budget = ValidateSceneBudget(plan)) return std::vector<Diagnostic>{*budget};
    plan.controls_ = SelectControls(controls, plan.instructions_);
    if (!plan.controls_.Definitions().empty() && !document.control_cues_.empty())
        plan.control_sequence_.emplace(plan.controls_, document.control_cues_);
    return plan;
}
}  // namespace rhythm::graph
