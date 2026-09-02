#include <cstddef>
#include <exception>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <CLI/CLI.hpp>

#include <bpe_builder.h>

namespace {

std::string read_text_file(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (not in) throw std::runtime_error("cannot open input file: " + path);
  std::string text(std::istreambuf_iterator<char>(in), {});
  if (in.bad()) throw std::runtime_error("read error on input file: " + path);
  return text;
}

// Parse the whitespace-separated list of token ids that tokenize_bpe writes.
// Every token must be a base-10 non-negative integer that fits in a token id;
// a signed parse would silently wrap "-1", so digits are checked up front.
std::vector<tokenizer::CP> parse_token_ids(const std::string& text) {
  std::vector<tokenizer::CP> ids;
  std::istringstream in(text);
  std::string tok;
  while (in >> tok) {
    for (char c : tok) {
      if (c < '0' or c > '9')
        throw std::runtime_error("not a token id: \"" + tok + "\"");
    }
    unsigned long long v = 0;
    try {
      v = std::stoull(tok);
    } catch (const std::exception&) {
      throw std::runtime_error("token id out of range: \"" + tok + "\"");
    }
    if (v > 0xFFFFFFFFULL)
      throw std::runtime_error("token id out of range: \"" + tok + "\"");
    ids.push_back(static_cast<tokenizer::CP>(v));
  }
  return ids;
}

} // namespace

int main(int argc, char** argv) {
  CLI::App app{"decode_bpe -- decode BPE token ids back into the original bytes"};

  std::string table_path;
  app.add_option("-t,--table", table_path, "merge table JSON file (from build_bpe)")
      ->required()
      ->check(CLI::ExistingFile);

  std::string input_path;
  app.add_option("-i,--input", input_path,
                 "file of whitespace-separated token ids (from tokenize_bpe)")
      ->required()
      ->check(CLI::ExistingFile);

  std::string output_path;
  app.add_option("-o,--output", output_path, "file to write the decoded bytes to")
      ->required();

  CLI11_PARSE(app, argc, argv);

  try {
    const auto merges = tokenizer::load_merge_table(table_path);

    auto decode_map = tokenizer::build_decode_map(merges);
    if (not decode_map) throw decode_map.error();

    const auto ids = parse_token_ids(read_text_file(input_path));

    auto decoded = tokenizer::decode(ids, *decode_map);
    if (not decoded) throw decoded.error();

    std::ofstream out(output_path, std::ios::binary | std::ios::trunc);
    if (not out) throw std::runtime_error("cannot open output file: " + output_path);
    out.write(decoded->data(), static_cast<std::streamsize>(decoded->size()));
    out.flush();
    if (not out) throw std::runtime_error("write failed: " + output_path);

    std::cout << "decoded " << ids.size() << " token(s) -> " << decoded->size()
              << " bytes; wrote " << output_path << "\n";
  } catch (const std::exception& e) {
    std::cerr << "decode_bpe: " << e.what() << "\n";
    return 1;
  }

  return 0;
}
