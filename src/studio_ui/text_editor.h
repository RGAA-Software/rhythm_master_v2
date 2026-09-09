#pragma once

#include <array>
#include <map>
#include <optional>
#include <string>

namespace rhythm::studio {
// Owns an uncommitted multiline/IME buffer. Invalid or cancelled input never
// mutates history or starts asynchronous font preparation.
class TextEditor final {
   public:
    std::optional<std::string> Draw(const std::string& source, const std::string& id,
                                    const std::map<std::string, std::string>& text);
    void Reset() {
        id_.clear();
        source_.clear();
        buffer_.fill(0);
        invalid_ = false;
    }

   private:
    std::string id_{};
    std::string source_{};
    std::array<char, 16385> buffer_{};
    bool invalid_ = false;
};
}  // namespace rhythm::studio
