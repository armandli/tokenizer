#include <bpe_builder.h>

#include <optional>

namespace tokenizer {

namespace {

s::vector<s::vector<CP>> convert_to_cp(const s::vector<s::string>& segments){
  s::vector<s::vector<CP>> ret;
  for (const s::string& segment : segments){
    s::vector<CP> cps;
    for (char c : segment){
      cps.push_back((unsigned char)c);
    }
    ret.push_back(cps);
  }
  return ret;
}

s::optional<MK> max_count(const umap<MK, ull>& counts){
  ull max_seen = 0U;
  MK max_key;
  for (const auto& [k, v] : counts){
    if (v > max_seen){
      max_key = k;
      max_seen = v;
    }
  }
  if (max_seen <= 1U) return s::nullopt;
  else                return max_key;
}

s::optional<MK> most_freq(const s::vector<s::vector<CP>>& segments){
  umap<MK, ull> counts;
  for (const s::vector<CP>& segment : segments){
    if (segment.size() <= 1) continue;
    for (size_t i = 0; i < segment.size() - 1; ++i){
      MK mk = (MK)segment[i] + ((MK)segment[i+1] << 32U);
      counts[mk]++;
    }
  }
  if (counts.size() == 0) return s::nullopt;
  else                    return max_count(counts);
}

void replace(s::vector<s::vector<CP>>& segments, MK key, CP val){
  for (s::vector<CP>& segment : segments){
    if (segment.size() <= 1) continue;
    size_t idx = 0;
    while (idx < segment.size() - 1){
      MK test = (MK)segment[idx] + ((MK)segment[idx+1] << 32U);
      if (test == key){
        segment[idx] = val;
        segment.erase(s::begin(segment) + idx + 1);
      }
      idx++;
    }
  }
}

} // namespace

umap<MK, CP> build_bpe_table(const s::vector<s::string>& segments, size_t max_merge){
  s::vector<s::vector<CP>> ss = convert_to_cp(segments);

  umap<MK, CP> ret;
  CP next_val = 256U;

  for (size_t i = 0; i < max_merge; ++i){
    s::optional<MK> next_key = most_freq(ss);

    if (not next_key.has_value()) break;

    ret[*next_key] = next_val;
    replace(ss, *next_key, next_val);
    next_val++;
  }

  return ret;
}

} // tokenizer
