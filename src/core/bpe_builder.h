#ifndef BPE_BUILDER
#define BPE_BUILDER

#include <vector>
#include <unordered_map>
#include <cstdint>
#include <regex>
#include <iostream>
#include <string>
#include <utility>
#include <expected>
#include <exception>

namespace s = std;

namespace tokenizer {

using CP = uint32_t;
using MK = uint64_t;
template <typename K, typename V> using umap = s::unordered_map<K, V>;

// A learned BPE merge table in application order: entry `i` is the (i+1)-th merge
// learned, its `.first` is the packed adjacent-pair key (low 32 bits = left
// token, high 32 bits = right token) and its `.second` is the new token id
// (== 256 + i).
using MergeTable = s::vector<s::pair<MK, CP>>;

using DecodeMap = umap<CP, s::vector<char>>;

struct table_error : s::runtime_error {
  table_error(const s::string& msg): s::runtime_error(msg) {}
};

struct decode_error : s::runtime_error {
  decode_error(const s::string& msg): s::runtime_error(msg) {}
};

// (left, right) adjacent-pair <-> packed MergeTable key.
inline MK pack_pair(CP left, CP right){
  return (MK)left | ((MK)right << 32U);
}
inline s::pair<CP, CP> unpack_pair(MK key){
  return {(CP)(key & 0xFFFFFFFFULL), (CP)(key >> 32U)};
}

MergeTable build_bpe_table(const s::vector<s::vector<char>>& segments, size_t max_merge);

s::vector<s::vector<char>> segment_corpus(const s::vector<char>& text, const s::regex& pattern);

s::vector<s::vector<char>> gpt2_presplit(const s::vector<char>& text);

MergeTable build_gpt2_table(const s::vector<char>& text, size_t max_merges);

s::vector<s::vector<char>> gpt4_presplit(const s::vector<char>& text);

MergeTable build_gpt4_table(const s::vector<char>& text, size_t max_merges);

s::vector<CP> tokenize(const s::vector<char>& text, const MergeTable& merges);

// Full id -> bytes map for decoding: every base byte 0..255 plus every merged id
// in `merges`, each expanded to its raw byte sequence.
s::expected<DecodeMap, table_error> build_decode_map(const MergeTable& merges);

s::expected<s::vector<char>, decode_error> decode(const s::vector<CP>& tokens, const DecodeMap& m);

void print_token_table(s::ostream& out, const MergeTable& merges);

// JSON round-trip for a merge table. Both throw s::runtime_error on an I/O
// failure, malformed JSON, or a schema mismatch. Implemented in merge_table_io.cpp.
void save_merge_table(const s::string& path, const MergeTable& merges);
MergeTable load_merge_table(const s::string& path);

} // tokenizer

#endif//BPE_BUILDER
