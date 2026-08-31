#include <bpe_builder.h>

#include <bpe_test_helpers.h>

#include <gtest/gtest.h>

#include <string>
#include <vector>

// Unit tests for tokenizer::build_bpe_table -- small hand-built corpora, one
// behaviour per test. See test/README.md for implementation notes (the
// minimum-frequency-2 threshold and the accepted tie-break limitation).

using bpe_test::left_of;
using bpe_test::merge_ids_in_order;
using bpe_test::pack;
using bpe_test::right_of;
using tokenizer::build_bpe_table;

namespace {

TEST(BpeBuilderTest, EmptyCorpusYieldsNoMerges) {
  EXPECT_TRUE(build_bpe_table({}, 10).empty());
}

TEST(BpeBuilderTest, SingleCharacterSegmentsYieldNoMerges) {
  EXPECT_TRUE(build_bpe_table({"a", "b", "c"}, 10).empty());
}

TEST(BpeBuilderTest, MaxMergeZeroYieldsNoMerges) {
  EXPECT_TRUE(build_bpe_table({"aaaa", "bbbb"}, 0).empty());
}

TEST(BpeBuilderTest, MergesTheMostFrequentPairFirst) {
  // (a,b) appears 3x, (b,a) once -> first merge is (a,b) with id 256.
  const auto table = build_bpe_table({"abab", "ab"}, 1);

  ASSERT_EQ(table.size(), 1u);
  ASSERT_EQ(table.count(pack('a', 'b')), 1u);
  EXPECT_EQ(table.at(pack('a', 'b')), 256u);
}

TEST(BpeBuilderTest, AssignsMergeIdsSequentiallyFrom256) {
  // Two 8-'a' segments collapse in three steps: (a,a) 14x -> 256,
  // (256,256) 6x -> 257, (257,257) 2x -> 258 (each still clears the freq-2 bar).
  const auto table = build_bpe_table({"aaaaaaaa", "aaaaaaaa"}, 8);

  EXPECT_EQ(merge_ids_in_order(table),
            (std::vector<tokenizer::CP>{256u, 257u, 258u}));
}

TEST(BpeBuilderTest, ChainsMergesIntoHigherOrderTokens) {
  // "aaaaaaaa": (a,a) 7x -> 256, then (256,256) 3x -> 257; (257,257) occurs once
  // so the chain stops there.
  const auto table = build_bpe_table({"aaaaaaaa"}, 8);

  ASSERT_EQ(table.size(), 2u);
  ASSERT_EQ(table.count(pack('a', 'a')), 1u);
  const tokenizer::CP aa = table.at(pack('a', 'a'));
  EXPECT_EQ(aa, 256u);
  ASSERT_EQ(table.count(pack(aa, aa)), 1u);
  EXPECT_EQ(table.at(pack(aa, aa)), 257u);
}

TEST(BpeBuilderTest, StopsEarlyWhenCorpusIsExhausted) {
  // Budget is 100 but the corpus only supports two merges before every segment
  // is a single token: (a,b) 4x -> 256, (256,256) 2x -> 257, then nothing left.
  const auto table = build_bpe_table({"abab", "abab"}, 100);

  EXPECT_EQ(table.size(), 2u);
}

TEST(BpeBuilderTest, NeverFormsPairsAcrossSegmentBoundaries) {
  // "ab" then "ba": if pairs spanned the boundary we would see a (b,b) merge.
  const auto table = build_bpe_table({"ab", "ba"}, 10);

  EXPECT_EQ(table.count(pack('b', 'b')), 0u);
  EXPECT_EQ(table.count(pack('a', 'a')), 0u);
  for (const auto& [key, val] : table) {
    (void)val;
    EXPECT_NE(left_of(key), right_of(key)) << "unexpected same-symbol merge";
  }
}

TEST(BpeBuilderTest, CountsOverlappingRunsOfIdenticalBytes) {
  // Characterization, not a bug: "aaa" contributes 2 to (a,a) because adjacent
  // pairs overlap. Mainstream trainers (subword-nmt) count the same way.
  const auto table = build_bpe_table({"aaa"}, 1);

  ASSERT_EQ(table.size(), 1u);
  EXPECT_EQ(table.count(pack('a', 'a')), 1u);
}

TEST(BpeBuilderTest, TreatsMultibyteCharactersAsBytes) {
  // Characterization: "éé" is U+00E9 twice = bytes C3 A9 C3 A9. The builder is
  // byte-level (by design), so the first merge glues the two bytes of one 'é'
  // (0xC3,0xA9). A codepoint-level tokenizer would instead merge (0xE9,0xE9).
  // The `CP` / `convert_to_cp` naming ("code point") is therefore misleading.
  const std::string ee("\xC3\xA9\xC3\xA9", 4);
  const auto table = build_bpe_table({ee}, 1);

  ASSERT_EQ(table.size(), 1u);
  ASSERT_EQ(table.count(pack(0xC3u, 0xA9u)), 1u);
  EXPECT_EQ(table.count(pack(0xE9u, 0xE9u)), 0u);
}

TEST(BpeBuilderTest, HandlesEmbeddedNulBytes) {
  const std::string with_nul("a\0a\0", 4);
  const auto table = build_bpe_table({with_nul}, 1);

  ASSERT_EQ(table.size(), 1u);
  // Pairs are (a,\0) x2 and (\0,a) x1 -> (a,\0) wins.
  EXPECT_EQ(table.count(pack('a', 0u)), 1u);
}

TEST(BpeBuilderTest, DoesNotMergePairsSeenOnlyOnce) {
  // The frequency-2 threshold: "abcdef" has no repeated pair, so nothing is
  // learned. (Regression guard for the minimum-frequency fix in max_count.)
  const auto table = build_bpe_table({"abcdef"}, 10);

  EXPECT_TRUE(table.empty())
      << "expected no merges from a corpus with no repetition, got "
      << table.size();
}

} // namespace
