# Tests

GoogleTest, split into two executables:

| Directory      | Target                        | ctest label   | Scope |
|----------------|-------------------------------|---------------|-------|
| `unit/`        | `tokenizer_unit_tests`        | `unit`        | One behaviour of `build_bpe_table` per test, on tiny hand-built corpora; `tokenize` / `print_token_table` against hand-built merge tables (`tokenize_test.cpp`, several cases are regression guards for specific bugs); `save_merge_table` / `load_merge_table` JSON round trips + rejection paths (`merge_table_io_test.cpp`); plus `MlxSmokeTest` (MLX headers compile and link into the test build). |
| `integration/` | `tokenizer_integration_tests` | `integration` | `build_bpe_table` end to end on a realistic corpus, whole-table properties, and a round trip through a small reference encoder. |

`build_bpe_table` returns `tokenizer::MergeTable` — a `std::vector<std::pair<MK,CP>>`
in learned order (entry `i` → id `256 + i`). `support/bpe_test_helpers.h` is
shared header-only glue over that: `pack` / `left_of` / `right_of` (wrappers for
`tokenizer::pack_pair` / `unpack_pair`), `has_merge` / `merged_id` /
`first_merge_key` / `merge_ids_in_order` lookups, a `repeated()` corpus builder,
and `apply_merges()` — a ~15-line reference BPE encoder that consumes the table.

**GoogleTest source:** the build uses a system GoogleTest (`brew install googletest`)
when one is found, and falls back to a pinned `FetchContent` copy otherwise. This
matters because MLX puts `/opt/homebrew/include` on the compile line, so a system
GoogleTest there would shadow a fetched copy's headers while the fetched library
is linked — an ABI mismatch. Using one consistent GoogleTest avoids it.

## Running

```sh
make test              # everything
make test-unit         # or: ctest --test-dir build -L unit --output-on-failure
make test-integration  # or: ctest --test-dir build -L integration --output-on-failure
```

Optional sanitizers: `cmake -S . -B build -DTOKENIZER_TEST_SANITIZERS=ON`.

## Implementation notes

- **Minimum-frequency threshold = 2.** `max_count()` merges a byte pair only if
  it occurs at least twice; `build_bpe_table` stops as soon as the most frequent
  remaining pair is below that (matches subword-nmt's default). Guarded by
  `BpeBuilderTest.DoesNotMergePairsSeenOnlyOnce`.

- **Accepted limitation — tie-breaking is not corpus-deterministic.** When
  several pairs tie for the top count, which one is merged depends on the
  `std::unordered_map<MK,ull>` count-map iteration order inside `most_freq` /
  `max_count`, so it can differ across STL implementations or when the pair set
  changes (rehash). Accepted for now.
  `BpePipelineTest.MergeTableIsReproducibleAcrossRuns` guards reproducibility
  within a single build (it uses a corpus whose top two pairs tie at 9x).

### Other findings (not blocking, no test)

- `most_freq()` recomputes every pair count from scratch each merge —
  `O(max_merge x corpus)`.
- Naming: `CP` / `convert_to_cp` say "code point" but the builder is byte-level
  (bytes `0..255` base vocab). Byte-level is a fine design; the name is
  misleading. Pinned by `BpeBuilderTest.TreatsMultibyteCharactersAsBytes`
  (characterization, not a bug).
