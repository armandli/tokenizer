#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace tokenizer {

// Minimal placeholder tokenizer: splits input on whitespace.
// Serves as the initial scaffold to be replaced with a real
// (e.g. BPE) tokenizer implementation.
class Tokenizer {
public:
    std::vector<std::string> tokenize(std::string_view text) const;
};

} // namespace tokenizer
