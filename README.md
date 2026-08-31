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

## Build & test

```sh
make            # configure + build (build/)
make test       # ctest: all tests
make test-unit
make test-integration
make run        # build/bin/tokenizer
```

Or directly:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## Layout

- `src/core` — `tokenizer_core` library (`build_bpe_table`). Links MLX publicly.
- `src/app` — `tokenizer` CLI (CLI11 for argument parsing).
- `test/` — GoogleTest suite, split into `unit/` and `integration/` (see
  `test/README.md`).
