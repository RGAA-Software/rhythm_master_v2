#include <algorithm>
#include <iostream>
#include <set>

#include "preview_routing.h"

namespace {
void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
}  // namespace
int main() {
    using namespace rhythm;
    try {
        studio::PreviewRouting routing;
        std::vector<graph::NodeId> roots;
        editor::ScopedViewers scope{{100}, {}};
        for (graph::NodeId id = 1; id <= 17; ++id) roots.push_back(id);
        for (graph::NodeId id = 1; id <= 9; ++id) scope.nodes_.push_back(id);
        auto request = routing.Prepare(roots, scope, "graph-a");
        Check(routing.Pages() == 4 && request.roots_.size() == 4 &&
                      request.scoped_.nodes_.size() == 4,
              "shared first-page admission");
        std::set<graph::NodeId> seen_roots, seen_scoped;
        for (std::size_t page = 0; page < 4; ++page) {
            request = routing.Prepare(roots, scope, "graph-a");
            Check(routing.Page() == page &&
                          request.roots_.size() + request.scoped_.nodes_.size() <= 8,
                  "stable bounded page");
            seen_roots.insert(request.roots_.begin(), request.roots_.end());
            seen_scoped.insert(request.scoped_.nodes_.begin(), request.scoped_.nodes_.end());
            routing.StepPage(1);
        }
        Check(seen_roots.size() == 17 && seen_scoped.size() == 9 && routing.Page() == 0,
              "every visible node reachable including equal IDs across scopes");
        Check(routing.TakePageChange() && !routing.TakePageChange(), "consume page change once");
        routing.StepPage(-1);
        Check(routing.Page() == 3, "previous wraps to final group");
        std::rotate(roots.begin(), roots.begin() + 10, roots.begin() + 11);
        request = routing.Prepare(roots, scope, "graph-a");
        Check(routing.Page() == 0 && request.roots_.front() == 11,
              "selection promoted to first group");
        routing.StepPage(1);
        scope.instance_path_ = {101};
        routing.Prepare(roots, scope, "graph-a");
        Check(routing.Page() == 0, "instance scope invalidates page");
        routing.StepPage(1);
        routing.Prepare(roots, scope, "graph-b");
        Check(routing.Page() == 0, "project identity invalidates page");
        request = routing.Prepare({1, 1, 2}, {{100}, {1, 1}}, "graph-b");
        Check(routing.Pages() == 1 && request.roots_.size() == 2 &&
                      request.scoped_.nodes_.size() == 1 && !routing.StepPage(1),
              "deduplicate within scope, keep cross-scope identity");
        routing.Prepare({}, {}, "graph-b");
        Check(!routing.Pages() && !routing.StepPage(-1), "empty demand");
        graph::Registry registry;
        graph::Document document;
        document.nodes_ = {registry.MakeNode(1, "texture.gradient"),
                           registry.MakeNode(2, "output.texture"),
                           registry.MakeNode(3, "event.beat")};
        document.output_ = 2;
        document.beat_grid_ = parameters::BeatSettings{};
        document.edges_ = {{1, 1, 2, "source"}};
        editor::Compilation compilation;
        compilation.viewers_ = {3};
        compilation.result_ = graph::Compile(document, registry, compilation.viewers_);
        compilation.scoped_nodes_ = {{7, 3}};
        compilation.authors_ = {{3, {{100}, 7}}};
        routing.Stage(compilation);
        Check(routing.Authors().empty(), "pending author map cannot address retained output");
        routing.Commit();
        Check(routing.Authors() == compilation.authors_,
              "author mapping commits with frame routing");
        compilation.authors_ = {{3, {{200}, 8}}};
        routing.Stage(compilation);
        Check(routing.Authors().at(3) == graph::AuthorNode{{100}, 7},
              "replacement preparation preserves the accepted author's identity");
        routing.Commit();
        Check(routing.Authors() == compilation.authors_, "replacement author map published once");
        Check(routing.SignalNodes().size() == 1 && routing.SignalNodes().front() == 3,
              "event routed into texture capture instead of numeric observation");
        studio::CanvasPreviews previews;
        previews.signals_[3].event_observation_ = runtime::EventObservation{4, 6, 2};
        const auto scoped = routing.Scoped(previews);
        Check(scoped.signals_.at(7).event_observation_->count_ == 4,
              "component-local event observation lost expanded identity mapping");
        std::cout << "preview groups: full reachability, shared budget, selection, scope and "
                     "project changes pass\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
