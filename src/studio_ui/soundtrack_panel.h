#pragma once

#include "rhythm/audio_ui/audio_panel.h"
#include "rhythm/content/music_authoring.h"

namespace rhythm::studio {
enum class SoundtrackAction { kBind, kAppend, kClear, kLoad };
class SoundtrackPanel final {
   public:
    bool Busy() const { return importer_ && importer_->Busy(); }
    std::optional<editor::Snapshot> Take(const editor::Snapshot& current, bool pending_edit,
                                         const audio_ui::AudioPanel& audio);
    void Sync(const editor::Snapshot& snapshot, const std::filesystem::path& assets,
              audio_ui::AudioPanel& audio);
    std::optional<SoundtrackAction> Draw(const editor::Snapshot& snapshot, bool selected,
                                         const std::map<std::string, std::string>& text);
    std::optional<editor::Snapshot> Start(SoundtrackAction action, const editor::Snapshot& snapshot,
                                          const std::filesystem::path& assets,
                                          audio_ui::AudioPanel& audio);

   private:
    std::optional<content::MusicAuthoring> importer_{};
    std::optional<media::Soundtrack> active_{};
    std::string document_{};
    std::string status_{};
    std::optional<std::filesystem::path> import_source_{};
    float import_gain_ = 1;
    bool import_loop_ = false;
};
}  // namespace rhythm::studio
