#include <arrow_io.h>
#include <bpe_builder.h>
#include <bpe_trainer.h>
#include <corpus_input.h>

#include <arrow_test_helpers.h>
#include <bpe_test_helpers.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <cstddef>
#include <iterator>
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

// A HuggingFace `datasets` split saved to disk: one .arrow shard per file plus
// JSON metadata sidecars. Training on the directory has to go through the Arrow
// reader and ignore the sidecars -- feeding `state.json` to the trainer would
// fold dataset bookkeeping into the merge table.
TEST(BpePipelineTest, TrainsOnArrowDatasetDirectoryIgnoringSidecars) {
  namespace fs = std::filesystem;

  const fs::path dir = fs::path(testing::TempDir()) / "bpe_pipeline_hf_split";
  fs::remove_all(dir);
  fs::create_directories(dir);

  const fs::path shard = dir / "data-00000-of-00001.arrow";
  arrow_test::write_arrow_fixture(
      shard.string(), arrow_test::Encoding::Stream,
      {{arrow_test::Row("low low low low low"), arrow_test::Row("lower lower")},
       {arrow_test::Row("newest newest newest newest newest newest"),
        arrow_test::Row("widest widest widest")}});
  {
    std::ofstream state(dir / "state.json", std::ios::binary | std::ios::trunc);
    state << R"({"_data_files":[{"filename":"data-00000-of-00001.arrow"}],"_split":"train"})";
    std::ofstream info(dir / "dataset_info.json", std::ios::binary | std::ios::trunc);
    info << R"({"builder_name":"parquet","features":{"text":{"dtype":"string"}}})";
  }

  // Train the way build_bpe does: enumerate, then pre-split each row on its own
  // so no merge spans a row boundary.
  const auto train = [](const std::vector<std::string>& input_paths) {
    std::vector<std::vector<char>> segments;
    for (const auto& file : tokenizer::expand_input_paths(input_paths)) {
      EXPECT_EQ(file.kind, tokenizer::InputKind::ArrowIpc) << file.path;
      for (const auto& row : tokenizer::read_arrow_column(file.path, "text")) {
        auto row_segments = tokenizer::gpt4_presplit(row);
        segments.insert(segments.end(), std::make_move_iterator(row_segments.begin()),
                        std::make_move_iterator(row_segments.end()));
      }
    }
    return build_bpe_table(segments, 10);
  };

  const auto from_dir = train({dir.string()});
  const auto from_shard = train({shard.string()});

  // The sidecars contributed nothing: naming the directory and naming the shard
  // learn the same table.
  ASSERT_FALSE(from_dir.empty());
  EXPECT_EQ(from_dir, from_shard);

  // And it is the canonical corpus's table -- (e,s) and (s,t) tie at 9x here
  // too, so the first merge is one of them.
  const auto first = bpe_test::first_merge_key(from_dir);
  ASSERT_TRUE(first.has_value());
  EXPECT_TRUE(*first == pack('e', 's') or *first == pack('s', 't'));
}

// build_bpe pre-splits Arrow rows across threads. Row order and the split between
// workers must not reach the table -- the per-thread segment counts are summed,
// not concatenated, so the result is the same whatever the slicing was.
TEST(BpePipelineTest, ArrowTrainingIsIndependentOfThreadCount) {
  namespace fs = std::filesystem;

  const fs::path dir = fs::path(testing::TempDir()) / "bpe_pipeline_threads";
  fs::remove_all(dir);
  fs::create_directories(dir);

  // Enough rows to slice several ways, sharing vocabulary so the per-thread maps
  // overlap rather than partitioning cleanly.
  std::vector<arrow_test::Row> rows;
  const char* vocabulary[] = {"the cat sat on the mat", "a cat and a hat",
                              "the mat and the hat", "cats sat on hats",
                              "the cat had a hat"};
  for (int r = 0; r < 400; ++r) rows.push_back(arrow_test::Row(vocabulary[r % 5]));

  const fs::path shard = dir / "data-00000-of-00001.arrow";
  arrow_test::write_arrow_fixture(shard.string(), arrow_test::Encoding::Stream, {rows});

  const auto rows_read = tokenizer::read_arrow_column(shard.string(), "text");
  ASSERT_EQ(rows_read.size(), 400u);

  const auto train = [&rows_read](std::size_t threads) {
    tokenizer::BpeTrainer trainer;
    trainer.add_texts(rows_read, threads);
    return trainer.build(120);
  };

  const auto single = train(1);
  ASSERT_FALSE(single.empty());
  for (const std::size_t threads : {2u, 3u, 7u, 16u}) {
    EXPECT_EQ(train(threads), single) << "threads=" << threads;
  }
}

} // namespace
