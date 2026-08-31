#ifndef BPE_BUILDER
#define BPE_BUILDER

#include <unordered_map>
#include <vector>
#include <string>
#include <cstdint>

namespace s = std;

namespace tokenizer {

using CP = uint32_t;
using MK = uint64_t;
using ull = uint64_t;
template <typename K, typename V> using umap = s::unordered_map<K, V>;

umap<MK, CP> build_bpe_table(const s::vector<s::string>& segments, size_t max_merge);

} // tokenizer

#endif//BPE_BUILDER
