#ifndef BPE_BUILDER
#define BPE_BUILDER

#include <unordered_map>
#include <vector>
#include <cstdint>
#include <regex>

namespace s = std;

namespace tokenizer {

using CP = uint32_t;
using MK = uint64_t;
using ull = uint64_t;
template <typename K, typename V> using umap = s::unordered_map<K, V>;

umap<MK, CP> build_bpe_table(const s::vector<s::vector<char>>& segments, size_t max_merge);

s::vector<s::vector<char>> segment_corpus(const s::vector<char>& text, const s::regex& pattern);

// Split `text` with a byte-level translation of an OpenAI pre-tokenizer regex
// (GPT-2 encoder.py / GPT-4 tiktoken cl100k_base). See bpe_builder.cpp for the
// exact patterns and their Unicode-property equivalence caveats.
s::vector<s::vector<char>> gpt2_presplit(const s::vector<char>& text);

umap<MK, CP> build_gpt2_table(const s::vector<char>& text, size_t max_merges);

s::vector<s::vector<char>> gpt4_presplit(const s::vector<char>& text);

umap<MK, CP> build_gpt4_table(const s::vector<char>& text, size_t max_merges);

} // tokenizer

#endif//BPE_BUILDER
