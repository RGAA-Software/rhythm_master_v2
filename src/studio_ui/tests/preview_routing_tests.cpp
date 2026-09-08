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
        std::cout << "preview groups: full reachability, shared budget, selection, scope and "
                     "project changes pass\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
