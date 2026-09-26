#include <arrow_io.h>

#include <arrow/io/file.h>

#include <arrow_test_helpers.h>

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <vector>

// Arrow IPC reading. Fixtures are written at run time into testing::TempDir()
// -- the repo checks in no binary data files, and writing them here also pins
// the exact encoding each case is meant to exercise.

using arrow_test::Encoding;
using arrow_test::Row;
using arrow_test::write_arrow_fixture;
using tokenizer::arrow_error;
using tokenizer::is_arrow_path;
using tokenizer::read_arrow_column;

namespace {

std::string temp_arrow(const std::string& name) {
  return testing::TempDir() + "arrow_io_" + name + ".arrow";
}

std::string to_string(const std::vector<char>& bytes) {
  return std::string(bytes.begin(), bytes.end());
}

std::vector<std::string> to_strings(const std::vector<std::vector<char>>& rows) {
  std::vector<std::string> out;
  for (const auto& row : rows) out.push_back(to_string(row));
  return out;
}

std::string write_fixture(const std::string& name, Encoding encoding,
                          const std::vector<std::vector<Row>>& batches) {
  const std::string path = temp_arrow(name);
  write_arrow_fixture(path, encoding, batches);
  return path;
}

} // namespace

TEST(ArrowIoTest, IsArrowPathMatchesOnExtension) {
  EXPECT_TRUE(is_arrow_path("data-00000-of-00002.arrow"));
  EXPECT_TRUE(is_arrow_path("/a/b/c.arrow"));

  EXPECT_FALSE(is_arrow_path("corpus.txt"));
  EXPECT_FALSE(is_arrow_path("state.json"));
  EXPECT_FALSE(is_arrow_path("shard.parquet"));
  EXPECT_FALSE(is_arrow_path("arrow"));
  // A bare extension is a dotfile, not an Arrow file.
  EXPECT_FALSE(is_arrow_path(".arrow"));
}

TEST(ArrowIoTest, ReadsStreamFormatAcrossBatches) {
  const auto path = write_fixture("stream_batches", Encoding::Stream,
                                  {{Row("alpha"), Row("beta")}, {Row("gamma")}});

  EXPECT_EQ(to_strings(read_arrow_column(path, "text")),
            (std::vector<std::string>{"alpha", "beta", "gamma"}));
}

TEST(ArrowIoTest, ReadsFileFormatAcrossBatches) {
  const auto path = write_fixture("file_batches", Encoding::File,
                                  {{Row("alpha"), Row("beta")}, {Row("gamma")}});

  EXPECT_EQ(to_strings(read_arrow_column(path, "text")),
            (std::vector<std::string>{"alpha", "beta", "gamma"}));
}

TEST(ArrowIoTest, PreservesEmptyAndNonUtf8Rows) {
  // Rows are raw bytes to the trainer: an empty row stays an empty segment
  // source, and a row that is not valid UTF-8 must survive byte for byte.
  const std::string raw("a\0b\xff", 4);
  const auto path =
      write_fixture("raw_bytes", Encoding::Stream, {{Row(""), Row(raw), Row("\n\n")}});

  const auto rows = read_arrow_column(path, "text");
  ASSERT_EQ(rows.size(), 3u);
  EXPECT_EQ(to_string(rows[0]), "");
  EXPECT_EQ(to_string(rows[1]), raw);
  EXPECT_EQ(to_string(rows[2]), "\n\n");
}

TEST(ArrowIoTest, SkipsNullRows) {
  const auto path = write_fixture("nulls", Encoding::Stream,
                                  {{Row("alpha"), std::nullopt, Row("gamma")}});

  EXPECT_EQ(to_strings(read_arrow_column(path, "text")),
            (std::vector<std::string>{"alpha", "gamma"}));
}

TEST(ArrowIoTest, ReadsEmptyFileAsNoRows) {
  const auto path = write_fixture("empty", Encoding::Stream, {});

  EXPECT_TRUE(read_arrow_column(path, "text").empty());
}

TEST(ArrowIoTest, RejectsUnknownColumn) {
  const auto path = write_fixture("unknown_column", Encoding::Stream, {{Row("alpha")}});

  try {
    read_arrow_column(path, "content");
    FAIL() << "expected arrow_error for a column that is not in the schema";
  } catch (const arrow_error& e) {
    const std::string what = e.what();
    // The message has to name both the miss and the alternatives -- a typo in
    // --column is the likeliest way to reach here.
    EXPECT_NE(what.find("content"), std::string::npos) << what;
    EXPECT_NE(what.find("\"text\""), std::string::npos) << what;
    EXPECT_NE(what.find("\"id\""), std::string::npos) << what;
  }
}

TEST(ArrowIoTest, RejectsNonStringColumn) {
  const auto path = write_fixture("wrong_type", Encoding::Stream, {{Row("alpha")}});

  EXPECT_THROW(read_arrow_column(path, "id"), arrow_error);
}

TEST(ArrowIoTest, RejectsMissingFile) {
  EXPECT_THROW(read_arrow_column(temp_arrow("does_not_exist"), "text"), arrow_error);
}

TEST(ArrowIoTest, RejectsFileThatIsNotArrow) {
  const std::string path = temp_arrow("not_arrow");
  auto sink = *arrow::io::FileOutputStream::Open(path);
  ASSERT_TRUE(sink->Write("this is plain text, not an arrow file\n").ok());
  ASSERT_TRUE(sink->Close().ok());

  EXPECT_THROW(read_arrow_column(path, "text"), arrow_error);
}
