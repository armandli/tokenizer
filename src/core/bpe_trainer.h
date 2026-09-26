#ifndef BPE_TRAINER_H
#define BPE_TRAINER_H

#include <bpe_builder.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace s = std;

namespace tokenizer {

// Learns a merge table without ever holding the corpus segment by segment.
//
// The corpus is collapsed to its DISTINCT pre-split segments, each carrying how
// often it occurred. That is exact, not an approximation: a pair's count is a
// plain sum over segments and a merge rewrites each segment independently, so a
// segment seen N times contributes exactly N times its own counts. On
// wikitext-103 it turns ~91 M segments into a few hundred thousand.
//
// Pair counts are then maintained incrementally. A merge only changes the
// segments that contain the merged pair, so only those are recounted, which
// replaces a full corpus rescan per merge with work proportional to what
// actually changed.
struct BpeTrainer {
  // Pre-splits `text` with the GPT-4 pre-tokenizer and folds its segments in.
  void add_text(const s::vector<char>& text);

  // Folds in one already-pre-split segment.
  void add_segment(const s::vector<char>& segment);

  // Same, over many buffers at once, pre-splitting on `threads` threads. Row
  // order does not affect the result, so this is deterministic regardless of
  // thread count.
  void add_texts(const s::vector<s::vector<char>>& rows, s::size_t threads);

  // Consumes the accumulated corpus and returns the merge table. On a count
  // tie the smallest packed pair key wins, which makes the table reproducible
  // across builds and standard-library versions.
  MergeTable build(s::size_t max_merge);

  // build() moves the accumulated segments from mPending into mWords, so
  // exactly one holds them; summing reports the same figure either side of it.
  s::size_t unique_segments() const { return mPending.size() + mWords.size(); }
  s::size_t total_segments() const { return mTotalSegments; }

protected:
  struct Word {
    s::vector<CP> tokens;
    s::uint64_t count;
  };

  // Ordered by count descending, then packed key ascending. Entries are never
  // updated in place; a stale one is recognised on pop because its count no
  // longer matches the live count for that key.
  struct Candidate {
    s::int64_t count;
    MK key;
  };

  struct ByCountThenKey {
    bool operator()(const Candidate& a, const Candidate& b) const {
      if (a.count != b.count) return a.count < b.count;
      return a.key > b.key;
    }
  };

  // Heterogeneous lookup so a segment can be looked up as a string_view into
  // the source buffer; only a miss allocates a key. Over 10^8 lookups that is
  // the difference between one temporary string per segment and none.
  struct SegmentHash {
    using is_transparent = void;
    s::size_t operator()(s::string_view v) const {
      return s::hash<s::string_view>{}(v);
    }
  };

  struct SegmentEq {
    using is_transparent = void;
    bool operator()(s::string_view a, s::string_view b) const { return a == b; }
  };

  using SegmentCounts =
      s::unordered_map<s::string, s::uint64_t, SegmentHash, SegmentEq>;

  // Walks `text` in place and folds its segments into `counts`.
  static void count_segments(const s::vector<char>& text, SegmentCounts& counts,
                             s::size_t& total);
  void fold_counts(const SegmentCounts& counts);
  void seed_pair_counts();
  // `sign` is +1 or -1; `index` also records the word against each of its
  // pairs.
  void apply_contribution(s::uint32_t word, s::int64_t sign, bool index);
  void publish_touched();

  s::int64_t live_count(MK key) const {
    const auto it = mPairCounts.find(key);
    return it == mPairCounts.end() ? 0 : it->second;
  }

  s::vector<Word> mWords;
  SegmentCounts mPending;
  s::size_t mTotalSegments = 0;

  umap<MK, s::int64_t> mPairCounts;
  umap<MK, s::vector<s::uint32_t>> mPairToWords;
  s::vector<Candidate> mHeapStore;
  s::vector<s::uint32_t> mWordStamp;
  s::vector<s::uint32_t> mAffected;
  s::vector<MK> mTouched;
};

} // tokenizer

#endif//BPE_TRAINER_H
