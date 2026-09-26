#ifndef BPE_INTERNAL_H
#define BPE_INTERNAL_H

#include <bpe_builder.h>

// The original, obviously-correct implementations, kept compiled so the
// optimized ones can be differentially tested against them. Not part of the
// public API and not called by any CLI -- if one of these disappears, the
// corresponding fast path loses its oracle.
namespace tokenizer {

// The merge sweep: replace every non-overlapping, left-to-right occurrence of
// `key` with `val`, in place. Shared by the reference trainer and BpeTrainer so
// the two cannot drift -- its exact semantics ("aaa" + (a,a) -> [A, a]) are
// what several tests pin.
void merge_pair(s::vector<CP>& seq, MK key, CP val);

// std::regex translation of the cl100k pattern, superseded in gpt4_presplit by
// the hand-written scanner in pretokenizer.cpp.
s::vector<s::vector<char>> gpt4_presplit_regex(const s::vector<char>& text);

// Full-recount trainer: O(max_merge x corpus), recomputing every pair count
// from scratch on each merge. Superseded in build_bpe_table by the
// deduplicated, incremental BpeTrainer.
MergeTable build_bpe_table_reference(const s::vector<s::vector<char>>& segments,
                                    size_t max_merge);

} // tokenizer

#endif//BPE_INTERNAL_H
