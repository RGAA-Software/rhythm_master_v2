#pragma once

#include <array>
#include <map>

#include "rhythm/geometry2d/vector.h"
#include "rhythm/runtime/runtime.h"

namespace rhythm::runtime::detail {
// Evaluation-thread CPU geometry cache. Color-only edits reuse tessellation;
// path identities/versions, projection, extent and stroke layout invalidate it.
class VectorMeshes final {
   public:
    void Retain(const graph::ExecutionPlan& plan);
    void Draw(const graph::Instruction& instruction, std::span<const NodeOutput> outputs,
              render::TextureHandle white, render::DrawList& list);
    std::size_t Size() const { return meshes_.size(); }
    std::uint64_t Builds() const { return builds_; }

   private:
    struct Key {
        graph::Operation operation_ = graph::Operation::kVectorFill;
        std::array<std::uint64_t, 4> sources_{};
        std::array<double, 8> layout_{};
        render::Extent extent_{};
        bool operator==(const Key&) const = default;
    };
    struct Entry {
        std::optional<Key> key_{};
        geometry2d::VectorMesh mesh_{};
    };
    std::map<graph::NodeId, Entry> meshes_{};
    std::uint64_t builds_ = 0;
};
}  // namespace rhythm::runtime::detail
