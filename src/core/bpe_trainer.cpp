#include <bpe_trainer.h>

#include <bpe_internal.h>
#include <pretokenizer.h>

#include <algorithm>
#include <thread>
#include <utility>

namespace tokenizer {

namespace {

s::size_t clamp_threads(s::size_t threads, s::size_t work){
  if (threads == 0) threads = 1;
  return s::min(threads, s::max<s::size_t>(work, 1U));
}

} // namespace

// No segment is materialised: gpt4_segment_length walks the buffer in place
// and the map is looked up by string_view, so only a segment seen for the first
// time allocates.
void BpeTrainer::count_segments(const s::vector<char>& text,
                                SegmentCounts& counts, s::size_t& total){
  const char* p = text.data();
  const s::size_t n = text.size();
  s::size_t i = 0;
  while (i < n){
    const s::size_t len = gpt4_segment_length(p, n, i);
    if (len == 0){
      ++i;
      continue;
    }
    const s::string_view segment(p + i, len);
    auto it = counts.find(segment);
    if (it == counts.end()) counts.emplace(s::string(segment), 1U);
    else                    ++it->second;
    ++total;
    i += len;
  }
}

void BpeTrainer::fold_counts(const SegmentCounts& counts){
  for (const auto& [segment, count] : counts){
    auto [it, inserted] = mPending.try_emplace(segment, count);
    if (not inserted) it->second += count;
  }
}

void BpeTrainer::add_text(const s::vector<char>& text){
  count_segments(text, mPending, mTotalSegments);
}

void BpeTrainer::add_segment(const s::vector<char>& segment){
  if (segment.empty()) return;
  const s::string_view view(segment.data(), segment.size());
  auto it = mPending.find(view);
  if (it == mPending.end()) mPending.emplace(s::string(view), 1U);
  else                      ++it->second;
  ++mTotalSegments;
}

void BpeTrainer::add_texts(const s::vector<s::vector<char>>& rows,
                           s::size_t threads){
  const s::size_t workers = clamp_threads(threads, rows.size());
  if (workers == 1 or rows.size() < 2){
    for (const auto& row : rows) add_text(row);
    return;
  }

  // Each worker counts a contiguous slice into its own map, so nothing is
  // shared until the reduce. Summing the per-worker counts afterwards does not
  // depend on how the rows were split, which is what makes the table
  // thread-count independent.
  s::vector<SegmentCounts> local(workers);
  s::vector<s::size_t> totals(workers, 0U);
  s::vector<s::thread> pool;
  pool.reserve(workers);

  const s::size_t per = (rows.size() + workers - 1U) / workers;
  for (s::size_t w = 0; w < workers; ++w){
    const s::size_t begin = s::min(w * per, rows.size());
    const s::size_t end = s::min(begin + per, rows.size());
    pool.emplace_back([&rows, &local, &totals, w, begin, end]{
      for (s::size_t r = begin; r < end; ++r){
        count_segments(rows[r], local[w], totals[w]);
      }
    });
  }
  for (auto& t : pool) t.join();

  for (s::size_t w = 0; w < workers; ++w){
    fold_counts(local[w]);
    mTotalSegments += totals[w];
  }
}

void BpeTrainer::apply_contribution(s::uint32_t word, s::int64_t sign,
                                    bool index){
  const Word& w = mWords[word];
  const s::int64_t delta = sign * (s::int64_t)w.count;
  for (s::size_t i = 0; i + 1U < w.tokens.size(); ++i){
    const MK key = pack_pair(w.tokens[i], w.tokens[i + 1U]);
    mPairCounts[key] += delta;
    mTouched.push_back(key);
    if (not index) continue;
    auto& holders = mPairToWords[key];
    // A pair can occur several times in one word ("aaa"); one entry is enough,
    // and consecutive duplicates are the only kind this loop can produce.
    if (holders.empty() or holders.back() != word) holders.push_back(word);
  }
}

void BpeTrainer::publish_touched(){
  s::sort(mTouched.begin(), mTouched.end());
  mTouched.erase(s::unique(mTouched.begin(), mTouched.end()), mTouched.end());

  // Every pair whose count moved gets a fresh candidate at its final value --
  // including ones that went DOWN. Skipping those would drop a pair that is
  // still frequent but whose only heap entry has just gone stale.
  for (const MK key : mTouched){
    const auto it = mPairCounts.find(key);
    if (it == mPairCounts.end()) continue;
    if (it->second <= 0){
      mPairCounts.erase(it);
      continue;
    }
    mHeapStore.push_back({it->second, key});
    s::push_heap(mHeapStore.begin(), mHeapStore.end(), ByCountThenKey{});
  }
  mTouched.clear();
}

void BpeTrainer::seed_pair_counts(){
  mWords.reserve(mPending.size());
  for (auto& [segment, count] : mPending){
    Word word;
    word.tokens.reserve(segment.size());
    for (const char c : segment) word.tokens.push_back((CP)(unsigned char)c);
    word.count = count;
    mWords.push_back(s::move(word));
  }
  SegmentCounts().swap(mPending);

  mWordStamp.assign(mWords.size(), 0U);
  for (s::uint32_t w = 0; w < (s::uint32_t)mWords.size(); ++w){
    apply_contribution(w, +1, true);
  }

  mTouched.clear();
  mHeapStore.reserve(mPairCounts.size());
  for (const auto& [key, count] : mPairCounts){
    if (count > 1) mHeapStore.push_back({count, key});
  }
  s::make_heap(mHeapStore.begin(), mHeapStore.end(), ByCountThenKey{});
}

MergeTable BpeTrainer::build(s::size_t max_merge){
  MergeTable ret;
  if (max_merge == 0) return ret;

  seed_pair_counts();

  for (s::size_t merge = 0; merge < max_merge; ++merge){
    MK best = 0;
    bool found = false;
    while (not mHeapStore.empty()){
      const Candidate top = mHeapStore.front();
      s::pop_heap(mHeapStore.begin(), mHeapStore.end(), ByCountThenKey{});
      mHeapStore.pop_back();
      if (live_count(top.key) != top.count) continue; // superseded
      if (top.count <= 1) break;                      // minimum frequency is 2
      best = top.key;
      found = true;
      break;
    }
    if (not found) break;

    const CP next_val = (CP)(256U + ret.size());
    ret.emplace_back(best, next_val);

    // The index can hold a word several times over, and can name words the pair
    // has since left; the stamp reduces it to each word once.
    const s::uint32_t stamp = (s::uint32_t)(merge + 1U);
    mAffected.clear();
    for (const s::uint32_t word : mPairToWords[best]){
      if (mWordStamp[word] == stamp) continue;
      mWordStamp[word] = stamp;
      mAffected.push_back(word);
    }

    for (const s::uint32_t word : mAffected){
      apply_contribution(word, -1, false);
    }
    for (const s::uint32_t word : mAffected){
      merge_pair(mWords[word].tokens, best, next_val);
      apply_contribution(word, +1, true);
    }

    // The left-to-right sweep leaves no occurrence of `best` behind, so its
    // count is now 0 and it must not be reconsidered.
    mPairCounts.erase(best);
    mPairToWords.erase(best);
    publish_touched();
  }

  return ret;
}

} // tokenizer
