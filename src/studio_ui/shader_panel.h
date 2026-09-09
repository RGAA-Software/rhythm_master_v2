#pragma once

#include <array>

#include "rhythm/editor/history.h"
#include "rhythm/image_shader/resources.h"
#include "rhythm/prepared_assets/loader.h"
#include "rhythm/shader_authoring/compiler.h"

namespace rhythm::studio {
class ShaderPanel final {
   public:
    void SetTools(shader_authoring::Toolchain tools) { tools_ = std::move(tools); }
    void Draw(const editor::Snapshot& snapshot, graph::NodeId selected,
              const std::filesystem::path& assets, const std::map<std::string, std::string>& text);
    std::optional<editor::Snapshot> Take(const editor::Snapshot& snapshot);
    bool Busy() const { return compiler_.Busy() || source_loader_.Busy(); }

   private:
    shader_authoring::Toolchain tools_{};
    shader_authoring::Compiler compiler_{};
    prepared_assets::Loader source_loader_{};
    std::uint64_t load_generation_ = 0;
    std::string document_{};
    graph::NodeId node_ = 0;
    std::string type_{};
    assets::AssetId asset_{};
    std::array<char, image_shader::kMaximumSourceBytes + 1> buffer_{};
    std::string error_{};
    std::optional<image_shader::Diagnostic> diagnostic_{};
    bool loaded_ = false;
    bool edited_ = false;
    struct Pending {
        std::string document_{};
        graph::NodeId node_ = 0;
        std::string type_{};
        assets::AssetId asset_{};
    };
    std::optional<Pending> pending_{};
};
}  // namespace rhythm::studio
