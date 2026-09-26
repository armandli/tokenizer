# Tests

GoogleTest, split into two executables:

| Directory      | Target                        | ctest label   | Scope |
|----------------|-------------------------------|---------------|-------|
| `unit/`        | `tokenizer_unit_tests`        | `unit`        | Differential checks of the pre-tokenizer scanner against the regex (`pretokenizer_test.cpp`) and of `BpeTrainer` against a full recount (`bpe_trainer_test.cpp`); one behaviour of `build_bpe_table` per test, on tiny hand-built corpora; `tokenize` / `print_token_table` against hand-built merge tables (`tokenize_test.cpp`, several cases are regression guards for specific bugs); `save_merge_table` / `load_merge_table` JSON round trips + rejection paths (`merge_table_io_test.cpp`); Arrow IPC reading in both encodings, including null/non-UTF-8 rows and the rejection paths (`arrow_io_test.cpp`); `-i` path expansion and how each file is classified (`corpus_input_test.cpp`); plus `MlxSmokeTest` (MLX headers compile and link into the test build). |
| `integration/` | `tokenizer_integration_tests` | `integration` | `build_bpe_table` end to end on a realistic corpus, whole-table properties, a round trip through a small reference encoder, and training straight off a HuggingFace-shaped Arrow dataset directory, including that the thread count does not change the table. |

`build_bpe_table` returns `tokenizer::MergeTable` — a `std::vector<std::pair<MK,CP>>`
in learned order (entry `i` → id `256 + i`). `support/bpe_test_helpers.h` is
shared header-only glue over that: `pack` / `left_of` / `right_of` (wrappers for
`tokenizer::pack_pair` / `unpack_pair`), `has_merge` / `merged_id` /
`first_merge_key` / `merge_ids_in_order` lookups, a `repeated()` corpus builder,
and `apply_merges()` — a ~15-line reference BPE encoder that consumes the table.

`support/arrow_test_helpers.h` is the other shared header: `write_arrow_fixture()`
writes a two-column (`text` string, `id` int32) `.arrow` file into
`testing::TempDir()` in either IPC encoding. No binary fixtures are checked in —
both suites build the ones they need at run time. This is the only reason the
test targets link Arrow directly; production code reads but never writes.

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

- **Minimum-frequency threshold = 2.** A byte pair is merged only if it occurs at
  least twice; training stops as soon as the most frequent remaining pair is below
  that (matches subword-nmt's default). Enforced in `BpeTrainer::build`, and in
  `max_count()` in the reference implementation. Guarded by
  `BpeBuilderTest.DoesNotMergePairsSeenOnlyOnce`.

- **Tie-breaking is deterministic: smallest packed pair key wins.** When several
  pairs tie for the top count, `BpeTrainer`'s candidate heap orders by count
  descending then key ascending, so the table is reproducible across builds and
  standard-library versions. Guarded by
  `BpeTrainerTest.TieIsBrokenOnTheSmallestPackedKey` (which also checks the
  answer does not depend on segment arrival order) and by
  `BpePipelineTest.MergeTableIsReproducibleAcrossRuns`.
  `BpePipelineTest.LearnsWellFormedTableOnCanonicalCorpus` still deliberately
  accepts either of the two 9x pairs, so it does not re-pin the rule.

- **Corpus memory is bounded by the DISTINCT segment count, not the corpus.**
  Training collapses the corpus to its distinct pre-split segments with counts,
  which is exact rather than approximate — a pair count is a sum over segments
  and a merge rewrites each segment independently, so a segment seen N times
  contributes N times its own counts. wikitext-103 `train` is 546 MB /
  108,286,982 segments / 587,146 distinct, and `build_bpe` peaks at ~750 MB
  resident on it. There is still no `--max-bytes`/`--max-rows` cap and none is
  needed at this scale.

### Differential tests are the real guard on the fast paths

Both optimized components are checked against a slow implementation rather than
against hand-written expectations, because the bugs they can have (a pair count
not decremented when the pair leaves a word; a whitespace run cut one byte off)
do not show up on small inputs:

- `pretokenizer_test.cpp` asserts the hand-written scanner is byte-identical to
  `gpt4_presplit_regex` (the `std::regex` version it replaced, kept compiled in
  `bpe_internal.h`) over 40,000 random strings plus targeted boundary cases.
  `gpt4_pattern_test.cpp`'s pinned cl100k expectations now run through the
  scanner too.
- `bpe_trainer_test.cpp` asserts `BpeTrainer` equals a full-recount reference
  exactly, over thousands of random corpora chosen to be tie-heavy and
  duplication-heavy. It also compares against the untouched original
  `build_bpe_table_reference` on a corpus with no ties.

Both suites pass with `-DTOKENIZER_SCALAR_PRETOKENIZER`, so the NEON path and
its scalar fallback are both covered.

### Other findings (not blocking, no test)

- Naming: `CP` / `convert_to_cp` say "code point" but the builder is byte-level
  (bytes `0..255` base vocab). Byte-level is a fine design; the name is
  misleading. Pinned by `BpeBuilderTest.TreatsMultibyteCharactersAsBytes`
  (characterization, not a bug).
