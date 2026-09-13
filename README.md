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

- `src/core` — `tokenizer_core` library (`build_bpe_table`). Links MLX publicly.
- `src/app` — CLIs (CLI11 for argument parsing), both GPT-4 pre-tokenizer:
  - `build_bpe -i input1 [-i input2 ...] -o merges.json -n max-merges` — learn a
    merge table from one or more files (each pre-split on its own, so merges
    never span a file boundary).
  - `tokenize_bpe -t merges.json -i input -o ids.txt` — encode a file into a
    space-separated list of BPE token ids.
  - `decode_bpe -t merges.json -i ids.txt -o output` — decode a token-id list
    (as written by `tokenize_bpe`) back into the original bytes.
  - `table_bpe -t merges.json` — print, for every learned token id starting at
    256, the text it decodes to.
- `test/` — GoogleTest suite, split into `unit/` and `integration/` (see
  `test/README.md`).
