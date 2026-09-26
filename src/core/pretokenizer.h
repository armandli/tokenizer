#ifndef PRETOKENIZER_H
#define PRETOKENIZER_H

#include <cstddef>
#include <vector>

namespace s = std;

namespace tokenizer {

// Hand-written byte scanner for the GPT-4 (cl100k_base) pre-tokenizer pattern,
// a drop-in replacement for running that pattern through std::regex, which
// manages only ~6 MB/s. Measured ~11x faster on wikitext-103 and asserted
// byte-identical to the regex in pretokenizer_test.cpp.
s::vector<s::vector<char>> gpt4_segments(const s::vector<char>& text);

// Length of the segment starting at `text[pos]`, or 0 when no alternative of
// the pattern matches there. A 0 means the caller advances one byte and emits
// nothing, which is what std::regex_iterator does with an unmatched position.
// (For this pattern every byte matches something, so 0 never actually occurs.)
s::size_t gpt4_segment_length(const char* text, s::size_t size, s::size_t pos);

} // tokenizer

#endif//PRETOKENIZER_H
