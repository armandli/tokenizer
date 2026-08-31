#include <cstdint>
#include <iostream>
#include <regex>
#include <string>
#include <vector>

#include <CLI/CLI.hpp>
#include <mlx/mlx.h>

#include <bpe_builder.h>

namespace mx = mlx::core;

int main(int argc, char** argv) {
  CLI::App app{"tokenizer -- learn a BPE merge table from text"};

  std::string text;
  app.add_option("-t,--text", text, "training text; whitespace splits segments")
      ->required();

  std::size_t max_merge = 50;
  app.add_option("-m,--max-merge", max_merge, "maximum number of merges to learn")
      ->capture_default_str();

  CLI11_PARSE(app, argc, argv);

  const std::vector<char> bytes(text.begin(), text.end());
  const auto segments = tokenizer::segment_corpus(bytes, std::regex{R"(\S+)"});
  const auto table = tokenizer::build_bpe_table(segments, max_merge);
  std::cout << "learned " << table.size() << " merge(s)\n";

  // Exercise MLX: load the learned token ids into an array and reduce on it.
  std::vector<std::int32_t> ids;
  for (const auto& [pair, id] : table) {
    (void)pair;
    ids.push_back(static_cast<std::int32_t>(id));
  }
  if (not ids.empty()) {
    mx::array id_arr(ids.begin(), {static_cast<int>(ids.size())}, mx::int32);
    mx::array lo = mx::min(id_arr);
    mx::array hi = mx::max(id_arr);
    mx::eval(lo, hi);
    std::cout << "token-id range: " << lo.item<std::int32_t>() << ".."
              << hi.item<std::int32_t>() << "\n";
  }

  return 0;
}
