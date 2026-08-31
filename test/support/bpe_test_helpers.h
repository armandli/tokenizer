#ifndef BPE_TEST_HELPERS_H
#define BPE_TEST_HELPERS_H

#include <bpe_builder.h>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Small helpers shared by the unit and integration test binaries. Kept
// header-only so both targets can just add the `support/` include dir.
namespace bpe_test {

using Table = tokenizer::umap<tokenizer::MK, tokenizer::CP>;

// build_bpe_table / segment_corpus deal in raw byte buffers now; this turns a
// test string literal into one so the call sites stay readable.
inline std::vector<char> bytes(std::string_view s) { return {s.begin(), s.end()}; }

// Pack an adjacent (left, right) token-id pair exactly the way bpe_builder does:
// low 32 bits = left token, high 32 bits = right token.
inline tokenizer::MK pack(std::uint32_t left, std::uint32_t right) {
  return static_cast<tokenizer::MK>(left) |
         (static_cast<tokenizer::MK>(right) << 32);
}

inline std::uint32_t left_of(tokenizer::MK key) {
  return static_cast<std::uint32_t>(key & 0xFFFFFFFFu);
}

inline std::uint32_t right_of(tokenizer::MK key) {
  return static_cast<std::uint32_t>(key >> 32);
}

// The merge learned first is the one with the smallest new id (256).
inline std::optional<tokenizer::MK> first_merge_key(const Table& table) {
  std::optional<tokenizer::MK> key;
  tokenizer::CP best = 0;
  for (const auto& [k, v] : table) {
    if (!key.has_value() || v < best) {
      key = k;
      best = v;
    }
  }
  return key;
}

// Merges in the order they were learned (ascending new id).
inline std::vector<std::pair<tokenizer::MK, tokenizer::CP>>
merges_by_rank(const Table& table) {
  std::vector<std::pair<tokenizer::MK, tokenizer::CP>> ordered(table.begin(),
                                                              table.end());
  std::sort(ordered.begin(), ordered.end(),
            [](const auto& a, const auto& b) { return a.second < b.second; });
  return ordered;
}

// Just the new ids, in learned order: for a well-formed table this is
// {256, 257, ..., 256 + size - 1}.
inline std::vector<tokenizer::CP> merge_ids_in_order(const Table& table) {
  std::vector<tokenizer::CP> ids;
  for (const auto& [key, val] : merges_by_rank(table)) {
    (void)key;
    ids.push_back(val);
  }
  return ids;
}

// Minimal reference BPE encoder: start from raw bytes, then apply each learned
// merge (in rank order) everywhere it matches, left to right, non-overlapping --
// the same sweep bpe_builder's internal `replace` performs. A real consumer of
// the table has to do exactly this, which is why it has to sort by rank first
// (the returned unordered_map has no order of its own).
inline std::vector<tokenizer::CP> apply_merges(const std::string& text,
                                               const Table& table) {
  std::vector<tokenizer::CP> toks;
  for (unsigned char c : text) toks.push_back(c);

  for (const auto& [key, val] : merges_by_rank(table)) {
    std::size_t i = 0;
    while (i + 1 < toks.size()) {
      if (pack(toks[i], toks[i + 1]) == key) {
        toks[i] = val;
        toks.erase(toks.begin() + static_cast<std::ptrdiff_t>(i + 1));
      }
      ++i;
    }
  }
  return toks;
}

// Repeat each segment `n` times -- a stand-in for word frequencies in a corpus.
inline std::vector<std::vector<char>> repeated(const std::vector<std::string>& words,
                                              const std::vector<int>& counts) {
  std::vector<std::vector<char>> out;
  for (std::size_t i = 0; i < words.size(); ++i) {
    for (int k = 0; k < counts[i]; ++k) out.push_back(bytes(words[i]));
  }
  return out;
}

} // namespace bpe_test

#endif // BPE_TEST_HELPERS_H
