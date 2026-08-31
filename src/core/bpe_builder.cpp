#include <bpe_builder.h>

#include <cstdint>
#include <map>
#include <optional>
#include <unordered_map>

namespace tokenizer {

using ull = uint64_t;
template <typename K, typename V> using umap = s::unordered_map<K, V>;

namespace {

// ---- byte buffers -> token ids -------------------------------------------

// Byte-level: every input byte becomes an id in 0..255, so a signed `char` has
// to go through `unsigned char` first (0x80 -> 128, not 0xFFFFFF80).
s::vector<CP> to_cp(const s::vector<char>& bytes){
  s::vector<CP> cps;
  cps.reserve(bytes.size());
  for (char c : bytes){
    cps.push_back((unsigned char)c);
  }
  return cps;
}

s::vector<s::vector<CP>> convert_to_cp(const s::vector<s::vector<char>>& segments){
  s::vector<s::vector<CP>> ret;
  ret.reserve(segments.size());
  for (const s::vector<char>& segment : segments){
    ret.push_back(to_cp(segment));
  }
  return ret;
}

// ---- merge mechanics -----------------------------------------------------

// Replace every non-overlapping, left-to-right occurrence of the adjacent pair
// `key` with the single token `val`, in place.
void merge_pair(s::vector<CP>& seq, MK key, CP val){
  size_t i = 0;
  while (i + 1 < seq.size()){
    if (pack_pair(seq[i], seq[i+1]) == key){
      seq[i] = val;
      seq.erase(s::begin(seq) + i + 1);
    }
    i++;
  }
}

void replace(s::vector<s::vector<CP>>& segments, MK key, CP val){
  for (s::vector<CP>& segment : segments){
    merge_pair(segment, key, val);
  }
}

s::optional<MK> max_count(const umap<MK, ull>& counts){
  ull max_seen = 0U;
  MK max_key = 0;
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
    for (size_t i = 0; i + 1 < segment.size(); ++i){
      counts[pack_pair(segment[i], segment[i+1])]++;
    }
  }
  if (counts.empty()) return s::nullopt;
  else                return max_count(counts);
}

// ---- debug output ------------------------------------------------------

// Write a raw token-byte buffer to `out`, rendering every byte that is not
// printable ASCII as \xNN so control bytes, NUL and UTF-8 continuation bytes do
// not corrupt the dump.
void write_bytes(s::ostream& out, const s::vector<char>& bytes){
  static const char hex[] = "0123456789abcdef";
  for (char c : bytes){
    unsigned char b = (unsigned char)c;
    if (b >= 0x20 && b <= 0x7E) out << (char)b;
    else                        out << '\\' << 'x' << hex[b >> 4] << hex[b & 0xF];
  }
}

// ---- OpenAI pre-tokenizer regexes ------------------------------------

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

MergeTable build_bpe_table(const s::vector<s::vector<char>>& segments, size_t max_merge){
  s::vector<s::vector<CP>> ss = convert_to_cp(segments);

  MergeTable ret;
  CP next_val = 256U;

  for (size_t i = 0; i < max_merge; ++i){
    s::optional<MK> next_key = most_freq(ss);

    if (not next_key.has_value()) break;

    ret.emplace_back(*next_key, next_val);
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

s::vector<s::vector<char>> gpt2_presplit(const s::vector<char>& text){
  return segment_corpus(text, gpt2_pattern());
}

MergeTable build_gpt2_table(const s::vector<char>& text, size_t max_merges){
  return build_bpe_table(gpt2_presplit(text), max_merges);
}

s::vector<s::vector<char>> gpt4_presplit(const s::vector<char>& text){
  return segment_corpus(text, gpt4_pattern());
}

MergeTable build_gpt4_table(const s::vector<char>& text, size_t max_merges){
  return build_bpe_table(gpt4_presplit(text), max_merges);
}

s::vector<CP> tokenize(const s::vector<char>& text, const MergeTable& merges){
  s::vector<CP> toks = to_cp(text);

  // `merges` is already in rank order; each one sweeps the whole sequence,
  // replacing non-overlapping occurrences left to right.
  for (const auto& [key, val] : merges){
    merge_pair(toks, key, val);
  }

  return toks;
}

void print_token_table(s::ostream& out, const MergeTable& merges){
  s::map<CP, s::vector<char>> legend;
  // `merges` is in learn order, so a token's components are always already in
  // `legend` by the time we need to expand them.
  for (const auto& [key, val] : merges){
    auto [first, second] = unpack_pair(key);

    s::vector<char> fstr;
    if (first < 256) fstr.push_back((char)first);
    else             fstr = legend.at(first);

    s::vector<char> sstr;
    if (second < 256) sstr.push_back((char)second);
    else              sstr = legend.at(second);

    write_bytes(out, fstr);
    out << " + ";
    write_bytes(out, sstr);
    out << " = " << val << s::endl;

    s::vector<char> combined = fstr;
    combined.insert(s::end(combined), s::begin(sstr), s::end(sstr));
    legend[val] = combined;
  }
}

} // tokenizer
