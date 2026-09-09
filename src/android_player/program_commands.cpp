#include <algorithm>

#include "program_bridge.h"

namespace rhythm::android_host {
bool ApplyProgramCommand(const ProgramCommand& command, player::PerformanceProgram& program,
                         std::span<const BuiltinWork> works,
                         const std::filesystem::path& canonical_cache,
                         std::optional<ImportFile>& owned_import) {
    if (command.action_ == 11) {
        program.Cancel();
        return true;
    }
    if (command.action_ == 10) {
        try {
            // A duplicate admission must not acquire a second deleting owner of
            // the file still read by the first worker. Rejected distinct copies
            // are claimed and reclaimed here, including busy-queue rejection.
            if (owned_import &&
                std::filesystem::weakly_canonical(command.path_) == owned_import->Path())
                return false;
            auto claimed = ImportFile::Claim(command.path_, canonical_cache);
            if (!claimed || !program.Import(claimed->Path(), command.duration_, command.mode_))
                return false;
            owned_import.emplace(std::move(*claimed));
            return true;
        } catch (const std::exception&) {
            return false;
        }
    }
    if (program.Busy()) return false;
    if (command.action_ == 1) return program.Load();
    if (command.action_ == 2) return program.Save();
    if (command.action_ == 3) return program.Resolve();
    auto list = program.Draft();
    if (command.action_ == 9) {
        const auto found = std::find_if(works.begin(), works.end(), [&](const auto& work) {
            return work.reference_.content_id_ == command.content_id_;
        });
        if (found == works.end()) return false;
        auto reference = found->reference_;
        reference.policy_ = command.follow_ ? performance::VersionPolicy::kCurrentBuiltin
                                            : performance::VersionPolicy::kExact;
        if (!list.Append({0, reference, command.title_, command.duration_, command.mode_}))
            return false;
    } else {
        const auto entries = list.Entries();
        const auto selected = std::find_if(entries.begin(), entries.end(), [&](const auto& entry) {
            return entry.id_ == command.id_;
        });
        if (selected == entries.end()) return false;
        const auto position = static_cast<std::size_t>(selected - entries.begin());
        auto entry = *selected;
        if (command.action_ == 4)
            list.Remove(entry.id_);
        else if (command.action_ == 5) {
            if (position == 0 || !list.Move(entry.id_, position - 1)) return false;
        } else if (command.action_ == 6) {
            if (!list.Move(entry.id_, position + 1)) return false;
        } else if (command.action_ == 7) {
            if (!list.Append(entry)) return false;
        } else if (command.action_ == 8) {
            entry.transition_seconds_ = command.duration_;
            entry.quantization_ = command.mode_;
            if (entry.work_.source_ == performance::WorkSource::kBuiltin)
                entry.work_.policy_ = command.follow_ ? performance::VersionPolicy::kCurrentBuiltin
                                                      : performance::VersionPolicy::kExact;
            if (!list.Replace(std::move(entry))) return false;
        } else
            return false;
    }
    return program.Edit(std::move(list));
}
}  // namespace rhythm::android_host
