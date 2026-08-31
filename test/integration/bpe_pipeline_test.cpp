#include <bpe_builder.h>

#include <bpe_test_helpers.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

// Integration tests: run build_bpe_table end to end on a realistic corpus and
// check whole-table properties, plus a round trip through a small reference
// encoder that consumes the table the way a real caller would.

using bpe_test::apply_merges;
using bpe_test::bytes;
using bpe_test::has_merge;
using bpe_test::merge_ids_in_order;
using bpe_test::pack;
using bpe_test::repeated;
using tokenizer::build_bpe_table;

namespace {

// The textbook BPE corpus: low x5, lower x2, newest x6, widest x3.
std::vector<std::vector<char>> canonical_corpus() {
  return repeated({"low", "lower", "newest", "widest"}, {5, 2, 6, 3});
}

TEST(BpePipelineTest, LearnsWellFormedTableOnCanonicalCorpus) {
  const std::size_t budget = 10;
  const auto table = build_bpe_table(canonical_corpus(), budget);

  ASSERT_FALSE(table.empty());
  EXPECT_LE(table.size(), budget);

  // Ids are dense, ordered, and start at 256; every merge has a distinct id.
  const auto ids = merge_ids_in_order(table);
  for (std::size_t i = 0; i < ids.size(); ++i) {
    EXPECT_EQ(ids[i], 256u + i);
  }

  // The first merge is one of the two most frequent byte pairs: (e,s) and (s,t)
  // each occur 9x (newest x6 + widest x3); every other pair occurs <= 8x. Which
  // of the two wins is not pinned here -- see BpeBuilderTest tie-break tests.
  const auto first = bpe_test::first_merge_key(table);
  ASSERT_TRUE(first.has_value());
  EXPECT_TRUE(*first == pack('e', 's') || *first == pack('s', 't'))
      << "first merge should be a maximum-frequency pair";

  // "est" gets built: whichever of e/s/t pair merged first (id 256), a later
  // merge combines it with the remaining letter.
  EXPECT_TRUE(has_merge(table, pack(256u, 't')) ||
              has_merge(table, pack('e', 256u)))
      << "expected a follow-on merge building \"est\"";
}

TEST(BpePipelineTest, MergeTableIsReproducibleAcrossRuns) {
  const auto a = build_bpe_table(canonical_corpus(), 12);
  const auto b = build_bpe_table(canonical_corpus(), 12);
  EXPECT_EQ(a, b);
}

TEST(BpePipelineTest, RespectsMaxMergeBudget) {
  const auto table = build_bpe_table(canonical_corpus(), 3);
  EXPECT_EQ(table.size(), 3u);
  EXPECT_EQ(merge_ids_in_order(table),
            (std::vector<tokenizer::CP>{256u, 257u, 258u}));
}

TEST(BpePipelineTest, EncoderRoundTripUsesLearnedMerges) {
  const auto table = build_bpe_table(canonical_corpus(), 12);

  // A word from the corpus tokenizes into fewer units than its raw bytes.
  const auto encoded = apply_merges("newest", table);
  EXPECT_GT(encoded.size(), 0u);
  EXPECT_LT(encoded.size(), std::string("newest").size());

  // A word not in the corpus still encodes without error and never expands.
  const auto unseen = apply_merges("lowest", table);
  EXPECT_LE(unseen.size(), std::string("lowest").size());
}

TEST(BpePipelineTest, HandlesHighBytesWithoutSignExtension) {
  // Bytes 0x80-0xFF must survive as 128..255, not sign-extend to ~0xFFFFFF80.
  const std::string high("\xC3\xA9\xC3\xA9\xC3\xA9", 6);
  const auto table = build_bpe_table({bytes(high)}, 2);

  ASSERT_FALSE(table.empty());
  const auto first = bpe_test::first_merge_key(table);
  ASSERT_TRUE(first.has_value());
  EXPECT_EQ(bpe_test::left_of(*first), 0xC3u);
  EXPECT_EQ(bpe_test::right_of(*first), 0xA9u);
}

} // namespace
