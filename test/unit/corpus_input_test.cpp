#include <corpus_input.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

// Input enumeration: which files a -i path expands to, and how each one is
// going to be decoded.

using tokenizer::expand_input_paths;
using tokenizer::InputFile;
using tokenizer::InputKind;

namespace fs = std::filesystem;

namespace {

// A directory that starts out empty, under testing::TempDir() so the suite
// leaves nothing behind in the source tree.
fs::path fresh_dir(const std::string& name) {
  const fs::path dir = fs::path(testing::TempDir()) / ("corpus_input_" + name);
  fs::remove_all(dir);
  fs::create_directories(dir);
  return dir;
}

void write_file(const fs::path& path, const std::string& contents) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << contents;
}

std::vector<std::string> paths_of(const std::vector<InputFile>& files) {
  std::vector<std::string> out;
  for (const auto& file : files) out.push_back(fs::path(file.path).filename().string());
  return out;
}

std::vector<InputKind> kinds_of(const std::vector<InputFile>& files) {
  std::vector<InputKind> out;
  for (const auto& file : files) out.push_back(file.kind);
  return out;
}

} // namespace

TEST(CorpusInputTest, ClassifiesNamedFilesByExtension) {
  const auto dir = fresh_dir("named_files");
  write_file(dir / "corpus.txt", "hello");
  write_file(dir / "shard.arrow", "not really arrow, only the name matters here");

  const auto files = expand_input_paths(
      {(dir / "corpus.txt").string(), (dir / "shard.arrow").string()});

  EXPECT_EQ(paths_of(files), (std::vector<std::string>{"corpus.txt", "shard.arrow"}));
  EXPECT_EQ(kinds_of(files), (std::vector<InputKind>{InputKind::Raw, InputKind::ArrowIpc}));
}

TEST(CorpusInputTest, PlainDirectoryYieldsEveryFileAsRawBytes) {
  const auto dir = fresh_dir("plain_dir");
  write_file(dir / "b.txt", "second");
  write_file(dir / "a.txt", "first");
  write_file(dir / "nested" / "c.txt", "third");

  const auto files = expand_input_paths({dir.string()});

  // Sorted, for a reproducible merge table.
  EXPECT_EQ(paths_of(files), (std::vector<std::string>{"a.txt", "b.txt", "c.txt"}));
  EXPECT_EQ(kinds_of(files), (std::vector<InputKind>(3, InputKind::Raw)));
}

TEST(CorpusInputTest, ArrowDatasetDirectorySkipsMetadataSidecars) {
  // The layout of a HuggingFace `datasets` split saved to disk.
  const auto dir = fresh_dir("hf_split");
  write_file(dir / "data-00000-of-00002.arrow", "shard one");
  write_file(dir / "data-00001-of-00002.arrow", "shard two");
  write_file(dir / "dataset_info.json", "{}");
  write_file(dir / "state.json", "{}");

  const auto files = expand_input_paths({dir.string()});

  EXPECT_EQ(paths_of(files), (std::vector<std::string>{"data-00000-of-00002.arrow",
                                                      "data-00001-of-00002.arrow"}));
  EXPECT_EQ(kinds_of(files), (std::vector<InputKind>(2, InputKind::ArrowIpc)));
}

TEST(CorpusInputTest, ArrowDetectionReachesNestedSplits) {
  // A dataset_dict directory: the .arrow files live one level down, per split.
  const auto dir = fresh_dir("hf_dict");
  write_file(dir / "dataset_dict.json", "{}");
  write_file(dir / "train" / "data-00000-of-00001.arrow", "train");
  write_file(dir / "train" / "state.json", "{}");
  write_file(dir / "validation" / "data-00000-of-00001.arrow", "validation");
  write_file(dir / "validation" / "state.json", "{}");

  const auto files = expand_input_paths({dir.string()});

  ASSERT_EQ(files.size(), 2u);
  EXPECT_EQ(fs::path(files[0].path).parent_path().filename(), "train");
  EXPECT_EQ(fs::path(files[1].path).parent_path().filename(), "validation");
  EXPECT_EQ(kinds_of(files), (std::vector<InputKind>(2, InputKind::ArrowIpc)));
}

TEST(CorpusInputTest, EmptyDirectoryYieldsNothing) {
  const auto dir = fresh_dir("empty_dir");

  EXPECT_TRUE(expand_input_paths({dir.string()}).empty());
}

TEST(CorpusInputTest, MixesDirectoryAndFileInputsInOrder) {
  const auto dir = fresh_dir("mixed");
  write_file(dir / "corpus" / "a.txt", "first");
  write_file(dir / "extra.txt", "second");

  const auto files =
      expand_input_paths({(dir / "corpus").string(), (dir / "extra.txt").string()});

  EXPECT_EQ(paths_of(files), (std::vector<std::string>{"a.txt", "extra.txt"}));
  EXPECT_EQ(kinds_of(files), (std::vector<InputKind>(2, InputKind::Raw)));
}
