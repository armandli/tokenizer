#include "tokenizer.h"

#include <cctype>

namespace tokenizer {

std::vector<std::string> Tokenizer::tokenize(std::string_view text) const {
    std::vector<std::string> tokens;
    std::size_t i = 0;
    while (i < text.size()) {
        while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i]))) {
            ++i;
        }
        std::size_t start = i;
        while (i < text.size() && !std::isspace(static_cast<unsigned char>(text[i]))) {
            ++i;
        }
        if (i > start) {
            tokens.emplace_back(text.substr(start, i - start));
        }
    }
    return tokens;
}

} // namespace tokenizer
