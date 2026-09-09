#include "rhythm/graph/text.h"

#include <utf8.h>

#include <bit>

#include "rhythm/graph/registry.h"

namespace rhythm::graph {
bool ValidText(std::string_view text) {
    if (text.size() > 16384 || !utf8::is_valid(text.begin(), text.end())) return false;
    std::size_t count = 0;
    for (auto position = text.begin(); position != text.end();) {
        const auto codepoint = utf8::next(position, text.end());
        if (++count > 4096 || (codepoint < 32 && codepoint != '\n' && codepoint != '\r') ||
            codepoint == 127)
            return false;
    }
    return true;
}
std::string TextImageKey(const Node& node) {
    std::string result = "text-v1/";
    for (const auto& [key, fallback] : {std::pair{"text_width", 512.0},
                                        {"text_height", 256.0},
                                        {"text_size", 48.0},
                                        {"text_spacing", 1.2},
                                        {"text_align", 0.0},
                                        {"text_wrap", 1.0}}) {
        result += std::to_string(std::bit_cast<std::uint64_t>(Scalar(node, key, fallback))) + '/';
    }
    const auto found = node.properties_.find("text_content");
    result += found == node.properties_.end() ? "Rhythm" : std::get<std::string>(found->second);
    return result;
}
}  // namespace rhythm::graph
