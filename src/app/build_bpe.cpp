#include <cstddef>
#include <exception>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <CLI/CLI.hpp>

#include <arrow_io.h>
#include <bpe_builder.h>
#include <bpe_trainer.h>
#include <corpus_input.h>

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
  CLI::App app{"build_bpe -- learn a BPE merge table from one or more files or directories (GPT-4 pre-tokenizer)"};

  std::vector<std::string> input_paths;
  app.add_option("-i,--input", input_paths,
                  "file(s) or directorie(s) to train the merge table on; directories are "
                  "searched recursively for files. A directory holding .arrow files is read "
                  "as an Arrow IPC dataset -- only those files are used, and any metadata "
                  "sidecars beside them are ignored")
      ->required()
      ->check(CLI::ExistingPath);

  std::string output_path;
  app.add_option("-o,--output", output_path, "file to write the merge table to (JSON)")
      ->required();

  std::size_t max_merges = 0;
  app.add_option("-n,--max-merges", max_merges, "maximum number of merges to produce")
      ->required();

  std::string column = "text";
  app.add_option("-c,--column", column,
                  "name of the string column to read from Arrow IPC (.arrow) inputs");

  std::size_t threads = std::thread::hardware_concurrency();
  if (threads == 0) threads = 1;
  app.add_option("-j,--threads", threads,
                  "threads to pre-split on; the merge table does not depend on this")
      ->check(CLI::PositiveNumber);

  CLI11_PARSE(app, argc, argv);

  try {
    const std::vector<tokenizer::InputFile> files = tokenizer::expand_input_paths(input_paths);
    if (files.empty()) throw std::runtime_error("no files found in the given input path(s)");

    // Each unit of input is pre-split on its own -- a whole file for raw bytes, a
    // single row for an Arrow column -- so neither the pre-tokenizer nor a
    // learned merge ever spans a unit boundary. Concatenating raw bytes first
    // could fuse the tail of one unit with the head of the next into a single
    // bogus segment.
    //
    // Segments go straight into the trainer, which keeps only the distinct ones
    // and their counts. Materializing them all first would cost ~10 GB and most
    // of the runtime on a corpus this size, for a list that is immediately
    // collapsed anyway.
    tokenizer::BpeTrainer trainer;
    std::size_t total_bytes = 0;
    std::size_t total_rows = 0;

    for (const auto& file : files) {
      switch (file.kind) {
        case tokenizer::InputKind::Raw: {
          const auto text = read_file(file.path);
          total_bytes += text.size();
          trainer.add_text(text);
        }

        break; case tokenizer::InputKind::ArrowIpc: {
          const auto rows = tokenizer::read_arrow_column(file.path, column);
          total_rows += rows.size();
          for (const auto& row : rows) total_bytes += row.size();
          trainer.add_texts(rows, threads);
        }

        break; default:
          throw std::runtime_error("unhandled input kind for: " + file.path);
      }
    }

    const std::size_t unique_segments = trainer.unique_segments();
    const std::size_t total_segments = trainer.total_segments();

    const auto table = trainer.build(max_merges);
    tokenizer::save_merge_table(output_path, table);
    std::cout << "learned " << table.size() << " merge(s) from " << files.size()
              << " file(s), " << total_rows << " row(s), " << total_bytes
              << " bytes, " << total_segments << " segment(s) (" << unique_segments
              << " distinct); wrote " << output_path << "\n";
  } catch (const std::exception& e) {
    std::cerr << "build_bpe: " << e.what() << "\n";
    return 1;
  }

  return 0;
}
