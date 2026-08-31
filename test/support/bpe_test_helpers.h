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

// build_bpe_table now returns an ordered vector -- entry i is the (i+1)-th merge
// learned, .first is the packed pair key, .second the new id (256 + i).
using Table = tokenizer::MergeTable;

// build_bpe_table / segment_corpus deal in raw byte buffers; this turns a test
// string literal into one so the call sites stay readable.
inline std::vector<char> bytes(std::string_view s) { return {s.begin(), s.end()}; }

// Pack an adjacent (left, right) token-id pair exactly the way bpe_builder does:
// low 32 bits = left token, high 32 bits = right token.
inline tokenizer::MK pack(std::uint32_t left, std::uint32_t right) {
  return tokenizer::pack_pair(left, right);
}

inline std::uint32_t left_of(tokenizer::MK key) { return tokenizer::unpack_pair(key).first; }
inline std::uint32_t right_of(tokenizer::MK key) { return tokenizer::unpack_pair(key).second; }

// Is there a merge for this packed pair key anywhere in the table?
inline bool has_merge(const Table& table, tokenizer::MK key) {
  return std::any_of(table.begin(), table.end(),
                     [&](const auto& e) { return e.first == key; });
}

// The new id assigned to this packed pair key, if the table has that merge.
inline std::optional<tokenizer::CP> merged_id(const Table& table, tokenizer::MK key) {
  for (const auto& [k, id] : table) {
    if (k == key) return id;
  }
  return std::nullopt;
}

// The packed key of the first merge learned (id 256).
inline std::optional<tokenizer::MK> first_merge_key(const Table& table) {
  if (table.empty()) return std::nullopt;
  return table.front().first;
}

// The new ids, in learned order: for a well-formed table this is
// {256, 257, ..., 256 + size - 1}.
inline std::vector<tokenizer::CP> merge_ids_in_order(const Table& table) {
  std::vector<tokenizer::CP> ids;
  for (const auto& [key, id] : table) {
    (void)key;
    ids.push_back(id);
  }
  return ids;
}

// Minimal reference BPE encoder: start from raw bytes, then apply each learned
// merge (in table order) everywhere it matches, left to right, non-overlapping --
// the same sweep bpe_builder's internal `replace` performs.
inline std::vector<tokenizer::CP> apply_merges(const std::string& text,
                                               const Table& table) {
  std::vector<tokenizer::CP> toks;
  for (unsigned char c : text) toks.push_back(c);

  for (const auto& [key, id] : table) {
    std::size_t i = 0;
    while (i + 1 < toks.size()) {
      if (pack(toks[i], toks[i + 1]) == key) {
        toks[i] = id;
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
