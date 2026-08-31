#include <bpe_builder.h>

#include <bpe_test_helpers.h>

#include <gtest/gtest.h>

#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

// Tests for tokenizer::save_merge_table / load_merge_table -- JSON persistence
// of a MergeTable. Round trips (hand-built, learned, empty), the exact on-disk
// format, and the rejection paths (missing file, bad JSON, wrong schema).

using bpe_test::bytes;
using bpe_test::pack;
using bpe_test::repeated;
using bpe_test::Table;
using tokenizer::build_bpe_table;
using tokenizer::load_merge_table;
using tokenizer::save_merge_table;
using tokenizer::tokenize;

namespace {

std::string temp_json(const std::string& name) {
  return testing::TempDir() + "merge_table_io_" + name + ".json";
}

std::string read_file(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(in), {});
}

void write_file(const std::string& path, const std::string& content) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << content;
}

TEST(MergeTableIoTest, RoundTripsHandBuiltTable) {
  const Table table{
      {pack('a', 'b'), 256}, {pack(256, 'c'), 257}, {pack('x', 'y'), 258}};
  const std::string path = temp_json("handbuilt");

  save_merge_table(path, table);
  EXPECT_EQ(load_merge_table(path), table);
}

TEST(MergeTableIoTest, RoundTripsEmptyTable) {
  const std::string path = temp_json("empty");
  save_merge_table(path, Table{});
  EXPECT_TRUE(load_merge_table(path).empty());
}

TEST(MergeTableIoTest, RoundTripsLearnedTableAndTokenizesIdentically) {
  const auto table = build_bpe_table(
      repeated({"low", "lower", "newest", "widest"}, {5, 2, 6, 3}), 12);
  ASSERT_FALSE(table.empty());

  const std::string path = temp_json("learned");
  save_merge_table(path, table);
  const auto loaded = load_merge_table(path);

  EXPECT_EQ(loaded, table);
  for (const char* w : {"newest", "widest", "lowest", "slowest"}) {
    EXPECT_EQ(tokenize(bytes(w), loaded), tokenize(bytes(w), table)) << "word: " << w;
  }
}

TEST(MergeTableIoTest, WritesStableJson) {
  const Table table{{pack('a', 'b'), 256}};
  const std::string path = temp_json("stable");
  save_merge_table(path, table);
  EXPECT_EQ(
      read_file(path),
      "{\"format\":\"tokenizer-bpe-merges\",\"version\":1,\"merges\":[[97,98,256]]}\n");
}

TEST(MergeTableIoTest, LoadRejectsMissingFile) {
  EXPECT_THROW(load_merge_table("/no/such/dir/does-not-exist.json"),
               std::runtime_error);
}

TEST(MergeTableIoTest, LoadRejectsMalformedJson) {
  const std::string path = temp_json("malformed");
  write_file(path, "{ this is not json ");
  EXPECT_THROW(load_merge_table(path), std::runtime_error);
}

TEST(MergeTableIoTest, LoadRejectsWrongSchema) {
  const std::string path = temp_json("wrongschema");
  write_file(path, R"({"format":"something-else","version":1,"merges":[]})");
  EXPECT_THROW(load_merge_table(path), std::runtime_error);
}

TEST(MergeTableIoTest, LoadRejectsMalformedMergeEntry) {
  const std::string path = temp_json("badentry");
  write_file(path,
             R"({"format":"tokenizer-bpe-merges","version":1,"merges":[[1,2]]})");
  EXPECT_THROW(load_merge_table(path), std::runtime_error);
}

} // namespace
