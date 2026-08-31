#include <bpe_builder.h>

#include <optional>

namespace tokenizer {

namespace {

s::vector<s::vector<CP>> convert_to_cp(const s::vector<s::vector<char>>& segments){
  s::vector<s::vector<CP>> ret;
  for (const s::vector<char>& segment : segments){
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

umap<MK, CP> build_bpe_table(const s::vector<s::vector<char>>& segments, size_t max_merge){
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

s::vector<s::vector<char>> segment_corpus(const s::vector<char>& text, const s::regex& pattern){
  s::vector<s::vector<char>> ret;

  // regex_iterator walks the range [begin, end); a byte 0 in `text` is just an
  // ordinary character to it, unlike a C string that would stop at the first NUL.
  using iter = s::vector<char>::const_iterator;
  s::regex_iterator<iter> it{s::begin(text), s::end(text), pattern};
  s::regex_iterator<iter> last;

  for (; it != last; ++it){
    const s::sub_match<iter>& m = (*it)[0]; // whole match
    ret.emplace_back(m.first, m.second);    // vector<char> straight from the match range
  }

  return ret;
}

namespace {

// Byte-level translations of the OpenAI pre-tokenizer regexes. std::regex
// ECMAScript has no \p{L}/\p{N} Unicode property classes, so:
//   \p{L}  -> [A-Za-z\x80-\xff]  (ASCII letters + every non-ASCII byte; UTF-8
//            lead/continuation bytes are all >= 0x80, so a whole multibyte char,
//            or a run of them, groups as one chunk)
//   \p{N}  -> [0-9]
//   \s     -> \s  (both OpenAI patterns run on engines whose \s == Unicode
//            White_Space -- GPT-2's `regex` module and tiktoken's Rust engine --
//            whose ASCII part is exactly [ \t\n\v\f\r], identical to C++ \s. The
//            ASCII controls 0x1c-0x1f are NOT whitespace, only punctuation.)
//   \s+(?!\S), {1,3}  -> kept verbatim (ECMAScript supports lookahead and
//            bounded repetition)
//
// Equivalence (checked against the literal patterns via the Python `regex`
// module over 120k+ full-byte fuzz strings, 0 mismatches): byte-identical for
// every single-byte / ASCII input. For UTF-8 the translation is faithful for
// letters and decimal digits; non-ASCII PUNCTUATION/SYMBOLS (and non-decimal
// number chars like superscripts) are grouped as letters. That divergence only
// ever merges segments -- it never splits a multibyte char and never fails.

// GPT-2 (OpenAI encoder.py, `import regex as re`):
//   's|'t|'re|'ve|'m|'ll|'d| ?\p{L}+| ?\p{N}+| ?[^\s\p{L}\p{N}]+|\s+(?!\S)|\s+
const s::regex& gpt2_pattern(){
  static const s::regex re(
    "'s|'t|'re|'ve|'m|'ll|'d"
    "| ?[A-Za-z\x80-\xff]+"
    "| ?[0-9]+"
    "| ?[^\\sA-Za-z0-9\x80-\xff]+"
    "|\\s+(?!\\S)"
    "|\\s+",
    s::regex_constants::ECMAScript);
  return re;
}

// GPT-4 (tiktoken cl100k_base):
//   '(?i:[sdmt]|ll|ve|re)|[^\r\n\p{L}\p{N}]?+\p{L}+|\p{N}{1,3}
//   | ?[^\s\p{L}\p{N}]++[\r\n]*|\s*[\r\n]|\s+(?!\S)|\s+
// (?i:...) has no std::regex equivalent -> expand to explicit case pairs.
// The ?+ / ++ possessive quantifiers -> plain ? / + : equivalent here, because
// the optional prefix (a non-letter) can never satisfy the following \p{L}+, and
// the ++ run is only followed by [\r\n]* (which always matches) with nothing
// after it -- so neither can usefully backtrack.
const s::regex& gpt4_pattern(){
  static const s::regex re(
    "'[sSdDmMtT]|'[lL][lL]|'[vV][eE]|'[rR][eE]"
    "|[^\\r\\nA-Za-z0-9\x80-\xff]?[A-Za-z\x80-\xff]+"
    "|[0-9]{1,3}"
    "| ?[^\\sA-Za-z0-9\x80-\xff]+[\\r\\n]*"
    "|\\s*[\\r\\n]"
    "|\\s+(?!\\S)"
    "|\\s+",
    s::regex_constants::ECMAScript);
  return re;
}

} // namespace

s::vector<s::vector<char>> gpt2_presplit(const s::vector<char>& text){
  return segment_corpus(text, gpt2_pattern());
}

umap<MK, CP> build_gpt2_table(const s::vector<char>& text, size_t max_merges){
  return build_bpe_table(gpt2_presplit(text), max_merges);
}

s::vector<s::vector<char>> gpt4_presplit(const s::vector<char>& text){
  return segment_corpus(text, gpt4_pattern());
}

umap<MK, CP> build_gpt4_table(const s::vector<char>& text, size_t max_merges){
  return build_bpe_table(gpt4_presplit(text), max_merges);
}


} // tokenizer
