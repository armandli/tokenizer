#include <cstddef>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

#include <CLI/CLI.hpp>

#include <bpe_builder.h>

namespace {

// Write a raw token-byte buffer to `out`, rendering every byte that is not
// printable ASCII as \xNN so control bytes, NUL and UTF-8 continuation bytes do
// not corrupt the printed table.
void write_bytes(std::ostream& out, const std::vector<char>& bytes) {
  static const char hex[] = "0123456789abcdef";
  for (char c : bytes) {
    unsigned char b = (unsigned char)c;
    if (b >= 0x20 && b <= 0x7E) out << (char)b;
    else                        out << '\\' << 'x' << hex[b >> 4] << hex[b & 0xF];
  }
}

} // namespace

int main(int argc, char** argv) {
  CLI::App app{"table_bpe -- print what each learned token id decodes to"};

  std::string table_path;
  app.add_option("-t,--table", table_path, "merge table JSON file (from build_bpe)")
      ->required()
      ->check(CLI::ExistingFile);

  CLI11_PARSE(app, argc, argv);

  try {
    const auto merges = tokenizer::load_merge_table(table_path);

    auto decode_map = tokenizer::build_decode_map(merges);
    if (not decode_map) throw decode_map.error();

    for (std::size_t i = 0; i < merges.size(); ++i) {
      const tokenizer::CP id = static_cast<tokenizer::CP>(256 + i);
      const auto iter = decode_map->find(id);
      if (iter == decode_map->end())
        throw std::runtime_error("no decoding for token id " + std::to_string(id));

      std::cout << id << ": ";
      write_bytes(std::cout, iter->second);
      std::cout << "\n";
    }
  } catch (const std::exception& e) {
    std::cerr << "table_bpe: " << e.what() << "\n";
    return 1;
  }

  return 0;
}
