# tokenizer

LLM tokenizer in C++.

## Prerequisites

- CMake ≥ 3.25, a C++26 compiler (Apple Clang / LLVM Clang).
- **MLX** (Apple silicon tensor library) — install via Homebrew: `brew install mlx`.
  The build locates it with `find_package(MLX)`; the Homebrew prefix is added to
  the search path automatically.
- **GoogleTest** — used from a system install if present (`brew install googletest`),
  otherwise fetched and pinned automatically. Only needed when building tests.
- **CLI11** — fetched automatically by CMake; nothing to install.
- **simdjson** — used from a system install if present (`brew install simdjson`),
  otherwise fetched and pinned automatically.
- **Apache Arrow** (C++) — required, install via Homebrew: `brew install apache-arrow`.
  Located with `find_package(Arrow)` off the same Homebrew prefix as MLX. Used
  only to read Arrow IPC (`.arrow`) dataset files; Parquet is not requested.
  Note the bottle pulls in a large transitive set (grpc, llvm, aws-sdk-cpp,
  thrift, …) — roughly 120 MB for Arrow itself plus its dependencies.
  Configuring prints a `Findlz4Alt.cmake` warning about a missing `lz4Config.cmake`.
  That comes from Arrow's own CMake package, which probes for a config Homebrew's
  lz4 does not ship and then falls back successfully — it is harmless.

## Build & test

```sh
make            # configure + build (build/)
make test       # ctest: all tests
make test-unit
make test-integration
make run        # build/bin/tokenize_bpe
```

Or directly:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## Layout

- `src/core` — `tokenizer_core` library: the trainer (`bpe_trainer.{h,cpp}`), the
  GPT-4 pre-tokenizer scanner (`pretokenizer.{h,cpp}`), the Arrow IPC reader and
  input enumeration. Links MLX publicly; simdjson and Arrow are private
  implementation details and appear in no header. `bpe_internal.h` declares the
  slow reference implementations the fast paths are differentially tested
  against — they are not part of the public API.

  Training deduplicates the corpus to its distinct pre-split segments and
  maintains pair counts incrementally, which is what makes a full wikitext-103
  split practical: 546 MB / 108 M segments collapses to 587 k distinct ones, and
  `-n 5000` takes ~4 s rather than ~4 h. The pre-tokenizer is a hand-written byte
  scanner rather than `std::regex` (~11x), with an ARM NEON path for long runs;
  build with `-DTOKENIZER_SCALAR_PRETOKENIZER` to force the scalar fallback.
- `src/app` — CLIs (CLI11 for argument parsing), both GPT-4 pre-tokenizer:
  - `build_bpe -i input1 [-i input2 ...] -o merges.json -n max-merges [-c column] [-j threads]`
    — learn a merge table from one or more files or directories. Each unit of
    input is pre-split on its own, so merges never span a unit boundary.

    `-j/--threads` (default: all cores) sets how many threads pre-split on. It
    affects speed only — the merge table is identical at any thread count.

    A directory is walked recursively. If the walk finds any `.arrow` file, the
    directory is read as an **Arrow IPC dataset**: only those files are used and
    anything beside them is ignored, which is what makes a HuggingFace
    `datasets` split work directly —

    ```sh
    build_bpe -i ~/data/wikitext-103-raw/train -o merges.json -n 5000
    ```

    reads the `data-*.arrow` shards and skips `state.json` / `dataset_info.json`.
    Each row of the column named by `-c/--column` (default `text`) is one unit;
    null rows are skipped. Both Arrow IPC encodings (stream, as `datasets`
    writes, and the `ARROW1` random-access file format) are detected from the
    file's own bytes. A directory with no `.arrow` file in it behaves as before:
    every regular file, read as raw bytes.
  - `tokenize_bpe -t merges.json -i input -o ids.txt` — encode a file into a
    space-separated list of BPE token ids.
  - `decode_bpe -t merges.json -i ids.txt -o output` — decode a token-id list
    (as written by `tokenize_bpe`) back into the original bytes.
  - `table_bpe -t merges.json` — print, for every learned token id starting at
    256, the text it decodes to.
- `test/` — GoogleTest suite, split into `unit/` and `integration/` (see
  `test/README.md`).
