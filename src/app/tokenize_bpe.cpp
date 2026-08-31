#include <cstddef>
#include <exception>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

#include <CLI/CLI.hpp>

#include <bpe_builder.h>

namespace {

std::vector<char> read_file(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (not in) throw std::runtime_error("cannot open input file: " + path);
  std::vector<char> bytes(std::istreambuf_iterator<char>(in), {});
  if (in.bad()) throw std::runtime_error("read error on input file: " + path);
  return bytes;
}

} // namespace

int main(int argc, char** argv) {
  CLI::App app{"tokenize_bpe -- encode a file into BPE token ids using a merge table"};

  std::string table_path;
  app.add_option("-t,--table", table_path, "merge table JSON file (from build_bpe)")
      ->required()
      ->check(CLI::ExistingFile);

  std::string input_path;
  app.add_option("-i,--input", input_path, "file to tokenize")
      ->required()
      ->check(CLI::ExistingFile);

  std::string output_path;
  app.add_option("-o,--output", output_path,
                 "file to write the space-separated token ids to")
      ->required();

  CLI11_PARSE(app, argc, argv);

  try {
    const auto merges = tokenizer::load_merge_table(table_path);
    const std::vector<char> input = read_file(input_path);

    // Same GPT-4 pre-tokenizer split build_bpe trains with, then BPE-merge each
    // segment (merges never cross a segment boundary).
    std::vector<tokenizer::CP> ids;
    for (const auto& segment : tokenizer::gpt4_presplit(input)) {
      const auto seg_ids = tokenizer::tokenize(segment, merges);
      ids.insert(ids.end(), seg_ids.begin(), seg_ids.end());
    }

    std::ofstream out(output_path, std::ios::binary | std::ios::trunc);
    if (not out) throw std::runtime_error("cannot open output file: " + output_path);
    for (std::size_t i = 0; i < ids.size(); ++i) {
      if (i) out << ' ';
      out << ids[i];
    }
    if (not ids.empty()) out << '\n';
    out.flush();
    if (not out) throw std::runtime_error("write failed: " + output_path);

    std::cout << "tokenized " << input.size() << " bytes -> " << ids.size()
              << " token(s); wrote " << output_path << "\n";
  } catch (const std::exception& e) {
    std::cerr << "tokenize_bpe: " << e.what() << "\n";
    return 1;
  }

  return 0;
}
