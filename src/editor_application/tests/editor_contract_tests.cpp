#include <chrono>
#include <iostream>
#include <stdexcept>

#include "rhythm/editor/commands.h"
#include "rhythm/editor/compiler_worker.h"
#include "rhythm/editor/history.h"

int main() {
    try {
        rhythm::editor::Snapshot initial;
        initial.document_.id_ = "history.test";
        rhythm::editor::History history(initial);
        rhythm::graph::Registry registry;
        auto graph = std::get<rhythm::editor::Snapshot>(
                rhythm::editor::AddNode(initial, registry, "texture.transform", {1, 2}, 1));
        graph = std::get<rhythm::editor::Snapshot>(
                rhythm::editor::AddNode(graph, registry, "texture.transform", {3, 4}, 2));
        graph = std::get<rhythm::editor::Snapshot>(
                rhythm::editor::Connect(graph, registry, 1, 2, "source"));
        const auto cycle = rhythm::editor::Connect(graph, registry, 2, 1, "source");
        if (std::get<rhythm::graph::Diagnostic>(cycle).code_ != "graph.cycle" ||
            graph.document_.edges_.size() != 1 || !initial.document_.nodes_.empty())
            throw std::runtime_error("commands.incomplete_cycle_or_mutation");
        if (!std::holds_alternative<rhythm::graph::Diagnostic>(
                    rhythm::editor::Connect(graph, registry, 1, 2, "invalid")))
            throw std::runtime_error("commands.invalid_port");
        auto next = initial;
        {
            auto named = std::get<rhythm::editor::Snapshot>(
                    rhythm::editor::DefineSignal(graph, registry, "shape", 1));
            named = std::get<rhythm::editor::Snapshot>(
                    rhythm::editor::BindInput(named, registry, 2, "source", "shape"));
            if (!named.document_.edges_.empty() || named.document_.bindings_.size() != 1)
                throw std::runtime_error("binding.did_not_replace_wire");
            if (!std::holds_alternative<rhythm::graph::Diagnostic>(
                        rhythm::editor::DefineSignal(named, registry, "shape", 2)))
                throw std::runtime_error("binding.retarget_cycle");
            if (!std::holds_alternative<rhythm::graph::Diagnostic>(
                        rhythm::editor::Connect(named, registry, 2, 1, "source")))
                throw std::runtime_error("binding.hidden_cycle");
            const auto wired = std::get<rhythm::editor::Snapshot>(
                    rhythm::editor::Connect(named, registry, 1, 2, "source"));
            if (!wired.document_.bindings_.empty() || wired.document_.edges_.size() != 1)
                throw std::runtime_error("binding.wire_precedence");
            rhythm::editor::History binding_history(named);
            const auto removed = rhythm::editor::RemoveSignal(named, "shape");
            if (!removed.document_.signals_.empty() || !removed.document_.bindings_.empty() ||
                !binding_history.Apply(removed, named.document_.revision_) ||
                !binding_history.Undo() ||
                binding_history.Current().document_.bindings_.size() != 1)
                throw std::runtime_error("binding.remove_undo");
        }
        const auto reserved = history.ReserveNodeId();
        next.title_ = "中文 title";
        if (!history.Apply(next, 0) || history.Apply(initial, 0) || !history.Undo() ||
            history.Current().title_ != "" || !history.Redo() ||
            history.Current().title_ != next.title_ || history.Current().document_.revision_ != 3)
            throw std::runtime_error("history.contract");
        if (history.ReserveNodeId() <= reserved) throw std::runtime_error("history.reused_id");
        next = history.Current();
        next.title_ = "branch";
        if (!history.Apply(next, 3) || history.Redo()) throw std::runtime_error("history.branch");
        {
            rhythm::editor::Snapshot content;
            content.document_.id_ = "template.local";
            content.document_.canvas_ = {720, 1280};
            content.document_.beat_grid_ = rhythm::parameters::BeatSettings{97, 6, 8, -0.25};
            content.document_.nodes_ = {registry.MakeNode(1, "texture.gradient"),
                                        registry.MakeNode(2, "output.texture"),
                                        registry.MakeNode(3, "control.scalar")};
            content.document_.edges_ = {{1, 3, 1, "amount"}};
            content.document_.control_titles_ = {{3, "Color mix"}};
            content.document_.control_snapshots_ = {{7, "Opening", {{3, 0.8}}}};
            content.document_.control_cues_ = {{9, "Opening cue", 0, 7, 0}};
            content.document_.output_ = 2;
            content.document_.signals_ = {{"image", 1}};
            content.document_.bindings_ = {{2, "source", "image"}};
            content.positions_ = {{1, {10, 20}}, {2, {200, 20}}};
            content.title_ = "Template";
            const auto before = history.Current();
            const std::vector<rhythm::graph::NodeId> ids{
                    history.ReserveNodeId(), history.ReserveNodeId(), history.ReserveNodeId()};
            const auto instantiated = std::get<rhythm::editor::Snapshot>(
                    rhythm::editor::InstantiateTemplate(before, content, ids));
            const auto compiled = rhythm::graph::Compile(instantiated.document_, registry);
            if (!std::holds_alternative<rhythm::graph::ExecutionPlan>(compiled))
                throw std::runtime_error("template.remapped_controls_do_not_compile");
            const auto& plan = std::get<rhythm::graph::ExecutionPlan>(compiled);
            if (plan.beat_grid_ != content.document_.beat_grid_ ||
                instantiated.document_.beat_grid_ != content.document_.beat_grid_)
                throw std::runtime_error("template.beat_grid");
            if (instantiated.document_.control_titles_ !=
                        std::map<rhythm::graph::NodeId, std::string>{{ids[2], "Color mix"}} ||
                instantiated.document_.control_snapshots_[0].values_ !=
                        rhythm::parameters::ControlValues{{ids[2], 0.8}} ||
                instantiated.document_.control_cues_ != content.document_.control_cues_ ||
                !plan.control_sequence_ || plan.control_sequence_->Sample(0).at(ids[2]) != 0.8)
                throw std::runtime_error("template.remapped_control_and_cue_semantics");
            if (instantiated.document_.id_ != before.document_.id_ ||
                instantiated.document_.output_ != ids[1] ||
                instantiated.positions_.at(ids[0]) != rhythm::editor::Position{10, 20} ||
                instantiated.document_.signals_.front().source_ != ids[0] ||
                instantiated.document_.bindings_.front().node_ != ids[1] ||
                !history.Apply(instantiated, before.document_.revision_) || !history.Undo() ||
                history.Current().title_ != before.title_ ||
                history.Current().document_.beat_grid_ != before.document_.beat_grid_ ||
                !history.Redo() ||
                history.Current().document_.canvas_ != content.document_.canvas_ ||
                history.Current().document_.beat_grid_ != content.document_.beat_grid_)
                throw std::runtime_error("template.undoable_instantiation");
            if (!std::holds_alternative<rhythm::graph::Diagnostic>(
                        rhythm::editor::InstantiateTemplate(history.Current(), content, ids)) ||
                content.document_.output_ != 2)
                throw std::runtime_error("template.identity_collision");
        }
        std::cout << "editor contracts passed\n";
        rhythm::editor::CompilerWorker worker;
        worker.Submit(initial.document_);
        auto latest = initial.document_;
        latest.revision_ = 99;
        const auto generation = worker.Submit(latest);
        std::optional<rhythm::editor::Compilation> completed;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!completed && std::chrono::steady_clock::now() < deadline) {
            completed = worker.Take();
            std::this_thread::yield();
        }
        if (!completed || completed->generation_ != generation ||
            !std::holds_alternative<std::vector<rhythm::graph::Diagnostic>>(completed->result_))
            throw std::runtime_error("compiler.latest_generation");
        rhythm::graph::ComponentDefinition component;
        component.type_ = "component.test.viewer";
        component.nodes_ = {registry.MakeNode(1, "texture.gradient"),
                            registry.MakeNode(2, "texture.shape")};
        component.output_ = 1;
        latest.components_ = {component};
        latest.nodes_ = {registry.MakeNode(10, component.type_, latest.components_),
                         registry.MakeNode(20, "output.texture")};
        latest.output_ = 20;
        latest.edges_ = {{1, 10, 20, "source"}};
        const auto scoped_generation = worker.Submit(latest, {10}, {{10}, {2}});
        completed.reset();
        const auto scoped_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!completed && std::chrono::steady_clock::now() < scoped_deadline) {
            completed = worker.Take();
            std::this_thread::yield();
        }
        if (!completed || completed->generation_ != scoped_generation ||
            !std::holds_alternative<rhythm::graph::ExecutionPlan>(completed->result_) ||
            completed->viewers_.size() != 2 || completed->scoped_nodes_.size() != 1 ||
            completed->viewers_.back() != completed->scoped_nodes_.at(2) ||
            std::get<rhythm::graph::ExecutionPlan>(completed->result_).instructions_.size() != 3)
            throw std::runtime_error("compiler.scoped_viewer_only_branch");
        if (completed->authors_.at(10) != rhythm::graph::AuthorNode{{10}, 1} ||
            completed->authors_.at(completed->scoped_nodes_.at(2)) !=
                    rhythm::graph::AuthorNode{{10}, 2} ||
            completed->authors_.at(20) != rhythm::graph::AuthorNode{{}, 20})
            throw std::runtime_error("compiler.authored_scope_mapping");
        const auto authors = completed->authors_;
        const auto root_generation = worker.Submit(latest);
        completed.reset();
        const auto root_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!completed && std::chrono::steady_clock::now() < root_deadline) {
            completed = worker.Take();
            std::this_thread::yield();
        }
        if (!completed || completed->generation_ != root_generation ||
            !std::holds_alternative<rhythm::graph::ExecutionPlan>(completed->result_) ||
            completed->authors_ != authors || !completed->scoped_nodes_.empty() ||
            std::get<rhythm::graph::ExecutionPlan>(completed->result_).instructions_.size() != 2)
            throw std::runtime_error("compiler.authors_without_preview_demand");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
