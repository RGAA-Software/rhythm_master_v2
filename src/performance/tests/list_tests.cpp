#include <iostream>
#include <limits>
#include <stdexcept>

#include "rhythm/performance/list.h"

namespace {
void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
template <typename Callback>
void Reject(Callback callback) {
    bool rejected = false;
    try {
        callback();
    } catch (const std::exception&) {
        rejected = true;
    }
    Check(rejected, "invalid list accepted");
}
}  // namespace
int main() {
    using namespace rhythm::performance;
    try {
        WorkReference work{WorkSource::kBuiltin,
                           "official.templates.aureate_vortex",
                           "0.4.0",
                           {std::string(64, 'a')},
                           VersionPolicy::kCurrentBuiltin};
        ListEntry entry{0, work, "鎏光流涡", 1.5, rhythm::parameters::Quantization::kBar};
        List list("演出");
        Check(list.Append(entry) == 1 && list.Append(entry) == 2 && list.Append(entry) == 3,
              "repeated works must have distinct entry identities");
        Check(list.Move(1, 2) && list.Entries()[2].id_ == 1 && list.Move(3, 0) &&
                      list.Entries()[0].id_ == 3,
              "reorder lost identity");
        Check(list.Remove(3) && list.Remove(2) && list.Remove(1), "remove failed");
        list = List(list.Title(), {}, list.LastId());
        Check(list.Append(entry) == 4, "reopen after delete reused entry identity");
        auto before = list;
        entry.transition_seconds_ = std::numeric_limits<double>::quiet_NaN();
        Check(!list.Append(entry) && list == before, "rejected append mutated list");
        Check(!list.Move(4, 1) && !list.Remove(99) && list == before, "invalid mutation");
        entry = list.Entries().front();
        entry.transition_seconds_ = 5;
        Check(list.Replace(entry) && list.Entries()[0].transition_seconds_ == 5, "edit failed");
        for (std::size_t index = 1; index < List::kMaximumItems; ++index)
            Check(list.Append(entry).has_value(), "list filled early");
        before = list;
        Check(!list.Append(entry) && list == before, "list exceeded preparation budget");
        Reject([&] { List("x", {entry, entry}); });
        Check(!List("x", {}, UINT64_MAX).Append(entry), "entry identity wrapped");
        std::vector<WorkReference> catalog{work};
        Check(Resolve(work, catalog).state_ == ResolutionState::kExact, "exact work missing");
        catalog[0].version_ = "0.2.0";
        catalog[0].package_.sha256_ = std::string(64, 'b');
        Check(Resolve(work, catalog).state_ == ResolutionState::kUpdated, "builtin upgrade lost");
        work.policy_ = VersionPolicy::kExact;
        Check(Resolve(work, catalog).state_ == ResolutionState::kChanged &&
                      !Resolve(work, catalog).catalog_index_,
              "pinned work silently replaced");
        catalog.push_back(catalog.front());
        Check(Resolve(work, catalog).state_ == ResolutionState::kAmbiguous, "duplicate chosen");
        catalog.clear();
        Check(Resolve(work, catalog).state_ == ResolutionState::kMissing, "missing work hidden");
        WorkReference managed{WorkSource::kManaged, {}, {}, {std::string(64, 'c')}};
        catalog = {managed};
        Check(Resolve(managed, catalog).state_ == ResolutionState::kExact, "managed hash lost");
        managed.policy_ = VersionPolicy::kCurrentBuiltin;
        Check(!ValidWork(managed), "managed work can change bytes");
        work.content_id_ = "../cache/staged.rhythmpack";
        Check(!ValidWork(work), "path accepted as persistent content ID");
        std::cout << "performance list identity, editing, bounds and catalog recovery passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
