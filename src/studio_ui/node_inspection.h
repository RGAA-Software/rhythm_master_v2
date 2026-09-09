#pragma once

#include "graph_canvas.h"

namespace rhythm::studio {
// Enlarges existing bounded preview captures; never starts another render path.
class NodeInspection final {
   public:
    void Show() { visible_ = true; }
    void Close() { visible_ = false; }
    void Draw(const graph::Document& document, graph::NodeId selected,
              const CanvasPreviews& previews, const std::map<std::string, std::string>& text);

   private:
    bool visible_ = false;
};
}  // namespace rhythm::studio
