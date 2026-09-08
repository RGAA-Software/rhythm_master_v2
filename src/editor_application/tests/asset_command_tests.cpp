#include <iostream>

#include "rhythm/editor/asset_commands.h"

namespace {
void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
}  // namespace
int main() {
    using namespace rhythm;
    try {
        editor::Snapshot source;
        source.document_.id_ = "assets.test";
        const assets::AssetRecord old{{std::string(64, 'a')}, 32, "image/png"};
        const assets::AssetRecord replacement{{std::string(64, 'b')}, 24, "image/jpeg"};
        source.assets_ = {old, replacement};
        graph::Node node;
        node.id_ = 1;
        node.type_ = "texture.image";
        node.properties_["asset"] = old.id_;
        node.extensions_ = old.id_.sha256_;
        source.document_.nodes_ = {node};
        graph::ComponentDefinition component;
        component.type_ = "component.inactive";
        component.nodes_ = {node};
        source.document_.components_ = {component};
        Check(editor::AssetUses(source, old.id_).size() == 2, "root and inactive component usages");
        Check(std::holds_alternative<graph::Diagnostic>(editor::RemoveUnusedAsset(source, old.id_)),
              "referenced asset cannot be deleted");
        const auto edit = editor::ReplaceAsset(source, old.id_, replacement);
        Check(std::holds_alternative<editor::Snapshot>(edit), "compatible image replacement");
        const auto next = std::get<editor::Snapshot>(edit);
        Check(next.assets_.size() == 1 && editor::AssetUses(next, old.id_).empty() &&
                      editor::AssetUses(next, replacement.id_).size() == 2,
              "rewrite and deduplicate");
        Check(next.document_.nodes_[0].extensions_ == old.id_.sha256_,
              "untyped text is not a reference");
        Check(editor::AssetUses(source, old.id_).size() == 2, "source unchanged");
        editor::History history(source);
        Check(history.Apply(next, source.document_.revision_) && history.Undo(),
              "undo replacement");
        Check(editor::AssetUses(history.Current(), old.id_).size() == 2 && history.Redo(),
              "redo replacement");
        auto wrong = replacement;
        wrong.media_type_ = "video/mp4";
        Check(std::holds_alternative<graph::Diagnostic>(
                      editor::ReplaceAsset(source, old.id_, wrong)),
              "media families cannot be crossed");
        auto conflict = replacement;
        ++conflict.bytes_;
        Check(std::holds_alternative<graph::Diagnostic>(
                      editor::ReplaceAsset(source, old.id_, conflict)),
              "same digest conflicting metadata rejected");
        auto music = source;
        music.assets_ = {{{std::string(64, 'a')}, 32, "audio/flac"}};
        music.soundtrack_.emplace();
        music.soundtrack_->asset_ = old.id_;
        media::AudioClip clip;
        clip.id_ = 7;
        clip.asset_ = old.id_;
        music.soundtrack_->clips_ = {clip};
        const assets::AssetRecord new_music{{std::string(64, 'c')}, 48, "audio/wav"};
        const auto audio_edit =
                std::get<editor::Snapshot>(editor::ReplaceAsset(music, old.id_, new_music));
        Check(audio_edit.soundtrack_->asset_ == new_music.id_ &&
                      audio_edit.soundtrack_->clips_[0].asset_ == new_music.id_ &&
                      editor::AssetUses(audio_edit, new_music.id_).size() == 4,
              "soundtrack and arrangement references follow atomic replacement");
        std::cout << "asset references, family compatibility, deduplication and undo pass\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
