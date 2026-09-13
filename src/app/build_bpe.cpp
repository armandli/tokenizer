#include <algorithm>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

#include <CLI/CLI.hpp>

#include <bpe_builder.h>

namespace fs = std::filesystem;

namespace {

std::vector<char> read_file(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (not in) throw std::runtime_error("cannot open input file: " + path);
  std::vector<char> bytes(std::istreambuf_iterator<char>(in), {});
  if (in.bad()) throw std::runtime_error("read error on input file: " + path);
  return bytes;
}

// Expands each input path into a flat list of file paths: a file is used as-is,
// a directory is walked recursively (in sorted order, for reproducible tables).
std::vector<std::string> expand_input_paths(const std::vector<std::string>& input_paths) {
  std::vector<std::string> files;
  for (const auto& input_path : input_paths) {
    const fs::path path(input_path);
    if (fs::is_directory(path)) {
      std::vector<fs::path> dir_files;
      for (const auto& entry : fs::recursive_directory_iterator(path)) {
        if (entry.is_regular_file()) dir_files.push_back(entry.path());
      }
      std::sort(dir_files.begin(), dir_files.end());
      for (auto& p : dir_files) files.push_back(p.string());
    } else {
      files.push_back(input_path);
    }
  }
  return files;
}

} // namespace

int main(int argc, char** argv) {
  CLI::App app{"build_bpe -- learn a BPE merge table from one or more files or directories (GPT-4 pre-tokenizer)"};

  std::vector<std::string> input_paths;
  app.add_option("-i,--input", input_paths,
                  "file(s) or directorie(s) to train the merge table on; directories are "
                  "searched recursively for files")
      ->required()
      ->check(CLI::ExistingPath);

  std::string output_path;
  app.add_option("-o,--output", output_path, "file to write the merge table to (JSON)")
      ->required();

  std::size_t max_merges = 0;
  app.add_option("-n,--max-merges", max_merges, "maximum number of merges to produce")
      ->required();

  CLI11_PARSE(app, argc, argv);

  try {
    const std::vector<std::string> files = expand_input_paths(input_paths);
    if (files.empty()) throw std::runtime_error("no files found in the given input path(s)");

    // Pre-split each file on its own, then combine the segments, so neither the
    // pre-tokenizer regex nor a learned merge ever spans a file boundary --
    // concatenating raw bytes first could fuse the tail of one file with the
    // head of the next into a single bogus segment.
    std::vector<std::vector<char>> segments;
    std::size_t total_bytes = 0;
    for (const auto& file : files) {
      const std::vector<char> corpus = read_file(file);
      total_bytes += corpus.size();
      auto file_segments = tokenizer::gpt4_presplit(corpus);
      segments.insert(segments.end(),
                       std::make_move_iterator(file_segments.begin()),
                       std::make_move_iterator(file_segments.end()));
    }

    const auto table = tokenizer::build_bpe_table(segments, max_merges);
    tokenizer::save_merge_table(output_path, table);
    std::cout << "learned " << table.size() << " merge(s) from " << files.size()
              << " file(s), " << total_bytes << " bytes; wrote " << output_path << "\n";
  } catch (const std::exception& e) {
    std::cerr << "build_bpe: " << e.what() << "\n";
    return 1;
  }

  return 0;
}
