#include <bpe_trainer.h>

#include <bpe_builder.h>
#include <bpe_internal.h>

#include <bpe_test_helpers.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <map>
#include <random>
#include <string>
#include <vector>

// BpeTrainer replaces a full corpus rescan per merge with incremental pair
// bookkeeping over deduplicated segments. The bookkeeping is where bugs hide --
// a count that is not decremented when a pair leaves a word, or a pair that
// drops out of the candidate heap while still frequent -- and neither shows up
// on small hand-written inputs.
//
// So the main test here is differential: a deliberately slow, obviously-correct
// full-recount implementation computes the same table, and the two must agree
// exactly on thousands of random corpora.

using bpe_test::bytes;
using tokenizer::BpeTrainer;
using tokenizer::CP;
using tokenizer::MergeTable;
using tokenizer::MK;

namespace {

// Recounts every pair from scratch on every merge and breaks ties on the
// smallest packed key, matching BpeTrainer's documented rule. std::map keeps
// the iteration ordered, so this is deterministic by construction. Deliberately
// the dumbest thing that can work -- it is the oracle, not production code.
MergeTable reference_table(const std::vector<std::vector<char>>& segments,
                           std::size_t max_merge) {
  std::vector<std::vector<CP>> corpus;
  corpus.reserve(segments.size());
  for (const auto& segment : segments) {
    std::vector<CP> tokens;
    tokens.reserve(segment.size());
    for (const char c : segment) tokens.push_back((CP)(unsigned char)c);
    corpus.push_back(std::move(tokens));
  }

  MergeTable out;
  for (std::size_t merge = 0; merge < max_merge; ++merge) {
    std::map<MK, std::uint64_t> counts;
    for (const auto& tokens : corpus) {
      for (std::size_t i = 0; i + 1 < tokens.size(); ++i) {
        ++counts[tokenizer::pack_pair(tokens[i], tokens[i + 1])];
      }
    }

    MK best = 0;
    std::uint64_t best_count = 0;
    for (const auto& [key, count] : counts) {
      if (count > best_count) { // strict: first (smallest) key wins a tie
        best = key;
        best_count = count;
      }
    }
    if (best_count <= 1) break;

    const CP next = (CP)(256U + out.size());
    out.emplace_back(best, next);
    for (auto& tokens : corpus) tokenizer::merge_pair(tokens, best, next);
  }
  return out;
}

MergeTable trained(const std::vector<std::vector<char>>& segments,
                   std::size_t max_merge) {
  BpeTrainer trainer;
  for (const auto& segment : segments) trainer.add_segment(segment);
  return trainer.build(max_merge);
}

void expect_matches_reference(const std::vector<std::vector<char>>& segments,
                             std::size_t max_merge) {
  const auto want = reference_table(segments, max_merge);
  const auto got = trained(segments, max_merge);

  ASSERT_EQ(got.size(), want.size())
      << "corpus of " << segments.size() << " segment(s)";
  for (std::size_t i = 0; i < got.size(); ++i) {
    const auto [wl, wr] = tokenizer::unpack_pair(want[i].first);
    const auto [gl, gr] = tokenizer::unpack_pair(got[i].first);
    EXPECT_EQ(got[i].first, want[i].first)
        << "merge " << i << ": got (" << gl << "," << gr << "), want ("
        << wl << "," << wr << ")";
    EXPECT_EQ(got[i].second, want[i].second) << "merge " << i;
  }
}

std::vector<std::vector<char>> random_corpus(std::mt19937& rng,
                                             std::size_t alphabet,
                                             std::size_t segments,
                                             std::size_t max_len) {
  std::uniform_int_distribution<std::size_t> letter(0U, alphabet - 1U);
  std::uniform_int_distribution<std::size_t> length(1U, max_len);

  std::vector<std::vector<char>> out;
  out.reserve(segments);
  for (std::size_t s = 0; s < segments; ++s) {
    std::vector<char> segment(length(rng));
    for (char& c : segment) c = (char)('a' + letter(rng));
    out.push_back(std::move(segment));
  }
  return out;
}

} // namespace

TEST(BpeTrainerTest, MatchesFullRecountOnRandomCorpora) {
  std::mt19937 rng(20260926U);
  // A small alphabet forces heavy duplication and frequent count ties, which is
  // exactly the regime the incremental path has to get right.
  for (const std::size_t alphabet : {2U, 3U, 5U, 12U, 26U}) {
    for (const std::size_t max_merge : {1U, 3U, 10U, 40U, 200U}) {
      for (int trial = 0; trial < 12; ++trial) {
        const auto corpus = random_corpus(rng, alphabet, 40U, 10U);
        expect_matches_reference(corpus, max_merge);
        if (::testing::Test::HasFatalFailure()) return;
      }
    }
  }
}

TEST(BpeTrainerTest, MatchesFullRecountOnLongSegments) {
  // Long segments mean a single merge rewrites a lot inside one word, and one
  // word can hold the same pair many times over.
  std::mt19937 rng(20260927U);
  for (const std::size_t alphabet : {2U, 4U}) {
    for (int trial = 0; trial < 20; ++trial) {
      const auto corpus = random_corpus(rng, alphabet, 8U, 120U);
      expect_matches_reference(corpus, 60U);
      if (::testing::Test::HasFatalFailure()) return;
    }
  }
}

TEST(BpeTrainerTest, MatchesFullRecountOnHeavilyDuplicatedCorpora) {
  // Duplication is the whole point of the word table; the weights must
  // reproduce the counts of the expanded corpus exactly.
  std::mt19937 rng(20260928U);
  for (int trial = 0; trial < 20; ++trial) {
    const auto distinct = random_corpus(rng, 4U, 6U, 12U);
    std::uniform_int_distribution<int> copies(1, 9);

    std::vector<std::vector<char>> corpus;
    for (const auto& segment : distinct) {
      const int n = copies(rng);
      for (int c = 0; c < n; ++c) corpus.push_back(segment);
    }
    expect_matches_reference(corpus, 50U);
    if (::testing::Test::HasFatalFailure()) return;
  }
}

TEST(BpeTrainerTest, MatchesFullRecountOnRunsOfIdenticalBytes) {
  // Overlapping counting ("aaa" contributes 2 to (a,a)) against non-overlapping
  // replacement ("aaa" becomes [A, a]) is the one place the two rules differ.
  for (std::size_t len = 1; len <= 40; ++len) {
    const std::vector<char> a(len, 'a');
    const std::vector<char> b(len, 'b');
    expect_matches_reference({a}, 20U);
    expect_matches_reference({a, a}, 20U);
    expect_matches_reference({a, b}, 20U);
    if (::testing::Test::HasFatalFailure()) return;
  }
}

TEST(BpeTrainerTest, MatchesFullRecountOnFullByteRange) {
  std::mt19937 rng(20260929U);
  std::uniform_int_distribution<int> byte(0, 255);
  std::uniform_int_distribution<std::size_t> length(1U, 12U);

  for (int trial = 0; trial < 300; ++trial) {
    std::vector<std::vector<char>> corpus;
    for (int s = 0; s < 30; ++s) {
      std::vector<char> segment(length(rng));
      for (char& c : segment) c = (char)(unsigned char)byte(rng);
      corpus.push_back(std::move(segment));
    }
    expect_matches_reference(corpus, 30U);
    if (::testing::Test::HasFatalFailure()) return;
  }
}

TEST(BpeTrainerTest, TieIsBrokenOnTheSmallestPackedKey) {
  // (a,b) and (c,d) both occur twice; (a,b) packs lower, so it must win, and
  // the answer must not depend on the order the segments arrive in.
  const std::vector<std::vector<char>> forward = {
      bytes("ab"), bytes("ab"), bytes("cd"), bytes("cd")};
  const std::vector<std::vector<char>> reversed = {
      bytes("cd"), bytes("cd"), bytes("ab"), bytes("ab")};

  const auto a = trained(forward, 1U);
  const auto b = trained(reversed, 1U);

  ASSERT_EQ(a.size(), 1U);
  EXPECT_EQ(a[0].first, bpe_test::pack('a', 'b'));
  EXPECT_EQ(a, b);
}

TEST(BpeTrainerTest, CountsSegmentsItWasGiven) {
  BpeTrainer trainer;
  trainer.add_text(bytes("the cat the cat the mat"));

  // Only the leading "the" has no space; the rest carry one. So the segments
  // are "the", " cat", " the", " cat", " the", " mat" -> 6 total, 4 distinct.
  EXPECT_EQ(trainer.total_segments(), 6U);
  EXPECT_EQ(trainer.unique_segments(), 4U);

  trainer.build(5U);
  EXPECT_EQ(trainer.total_segments(), 6U);
  EXPECT_EQ(trainer.unique_segments(), 4U);
}

TEST(BpeTrainerTest, CountsNothingForEmptyInput) {
  BpeTrainer trainer;
  trainer.add_text({});
  trainer.add_segment({});

  EXPECT_EQ(trainer.total_segments(), 0U);
  EXPECT_EQ(trainer.unique_segments(), 0U);
  EXPECT_TRUE(trainer.build(10U).empty());
}

TEST(BpeTrainerTest, ThreadCountDoesNotChangeTheTable) {
  std::mt19937 rng(20260930U);
  std::uniform_int_distribution<std::size_t> pick(0U, 25U);

  // Enough rows that every worker gets a slice, with repeated vocabulary so the
  // per-thread maps genuinely overlap and have to be summed, not concatenated.
  std::vector<std::vector<char>> rows;
  for (int r = 0; r < 500; ++r) {
    std::string row;
    for (int w = 0; w < 30; ++w) {
      row += ' ';
      const std::size_t len = 1U + (pick(rng) % 6U);
      for (std::size_t c = 0; c < len; ++c) row += (char)('a' + pick(rng) % 6U);
    }
    rows.push_back(bytes(row));
  }

  MergeTable single;
  std::size_t single_total = 0;
  std::size_t single_unique = 0;
  {
    BpeTrainer trainer;
    trainer.add_texts(rows, 1U);
    single_total = trainer.total_segments();
    single_unique = trainer.unique_segments();
    single = trainer.build(200U);
  }
  ASSERT_FALSE(single.empty());

  for (const std::size_t threads : {2U, 3U, 5U, 8U, 12U, 64U}) {
    BpeTrainer trainer;
    trainer.add_texts(rows, threads);
    EXPECT_EQ(trainer.total_segments(), single_total) << "threads=" << threads;
    EXPECT_EQ(trainer.unique_segments(), single_unique)
        << "threads=" << threads;
    EXPECT_EQ(trainer.build(200U), single) << "threads=" << threads;
  }
}

TEST(BpeTrainerTest, AddTextAgreesWithPreSplitSegments) {
  // Feeding raw text and feeding its pre-split segments must train the same
  // table -- this is what lets build_bpe_table stay a thin wrapper.
  const auto text = bytes("the cat sat on the mat, the cat sat on the hat");
  const auto segments = tokenizer::gpt4_presplit(text);

  BpeTrainer from_text;
  from_text.add_text(text);

  EXPECT_EQ(from_text.build(30U), trained(segments, 30U));
}

TEST(BpeTrainerTest, MaxMergeZeroYieldsNoMerges) {
  EXPECT_TRUE(trained({bytes("aaaa"), bytes("bbbb")}, 0U).empty());
}

TEST(BpeTrainerTest, AgreesWithTheOriginalTrainerOnATieFreeCorpus) {
  // The original implementation left tie-breaking to unordered_map order, so it
  // can only be compared where every merge has a strict maximum. This corpus is
  // built so each step does.
  const std::vector<std::vector<char>> corpus = {
      bytes("aaaaaaaa"), bytes("aaaaaaaa"), bytes("bbbb"), bytes("cc")};

  EXPECT_EQ(trained(corpus, 10U),
            tokenizer::build_bpe_table_reference(corpus, 10U));
}
