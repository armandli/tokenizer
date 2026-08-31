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
  CLI::App app{"build_bpe -- learn a BPE merge table from a file (GPT-4 pre-tokenizer)"};

  std::string input_path;
  app.add_option("-i,--input", input_path, "file to train the merge table on")
      ->required()
      ->check(CLI::ExistingFile);

  std::string output_path;
  app.add_option("-o,--output", output_path, "file to write the merge table to (JSON)")
      ->required();

  std::size_t max_merges = 0;
  app.add_option("-n,--max-merges", max_merges, "maximum number of merges to produce")
      ->required();

  CLI11_PARSE(app, argc, argv);

  try {
    const std::vector<char> corpus = read_file(input_path);
    const auto table = tokenizer::build_gpt4_table(corpus, max_merges);
    tokenizer::save_merge_table(output_path, table);
    std::cout << "learned " << table.size() << " merge(s) from " << corpus.size()
              << " bytes; wrote " << output_path << "\n";
  } catch (const std::exception& e) {
    std::cerr << "build_bpe: " << e.what() << "\n";
    return 1;
  }

  return 0;
}
