#include <pretokenizer.h>

#include <cstdint>

// TOKENIZER_SCALAR_PRETOKENIZER forces the scalar path on a machine that has
// NEON, so the fallback is measurable and testable rather than dead code.
#if defined(__ARM_NEON) and not defined(TOKENIZER_SCALAR_PRETOKENIZER)
#define TOKENIZER_PRETOKENIZER_NEON 1
#include <arm_neon.h>
#endif

// Byte-level scanner for the GPT-4 (cl100k_base) pre-tokenizer. It reproduces
// the std::regex translation in bpe_builder.cpp byte for byte:
//
//   '[sSdDmMtT] | '[lL][lL] | '[vV][eE] | '[rR][eE]           (1) contractions
//   [^\r\nA-Za-z0-9\x80-\xff]?[A-Za-z\x80-\xff]+              (2) word
//   [0-9]{1,3}                                                (3) digit group
//    ?[^\sA-Za-z0-9\x80-\xff]+[\r\n]*                         (4) symbol run
//   \s*[\r\n]                                                 (5) newline run
//   \s+(?!\S)                                                 (6) held-back space
//   \s+                                                       (7) space run
//
// Alternatives are tried in this order and the first to match wins, which is
// ECMAScript leftmost-alternation -- the same thing std::regex_iterator does.
//
// Two alternatives are only correct once their backtracking is unfolded, and
// both are load-bearing for real text:
//
//   (5) `\s*` is greedy and then gives characters back one at a time until
//       `[\r\n]` matches, so the match runs up to and including the LAST CR/LF
//       inside the whitespace run -- not the first, and not the whole run.
//   (6) same give-back, but the guard `(?!\S)` only holds at end of input or
//       before another space. So a run that reaches end of input matches whole;
//       otherwise it yields its last character to the following word and needs
//       at least 2 characters to match at all.
//
// Together those two are why "a   b" splits as {"a", "  ", " b"} and
// "mix \t\n \t run" as {"mix", " \t\n", " \t", " run"}.

namespace tokenizer {

namespace {

enum : s::uint8_t {
  CL_LETTER = 1U, // A-Za-z, plus every byte >= 0x80 (see bpe_builder.cpp)
  CL_DIGIT = 2U,
  CL_SPACE = 4U, // std::regex \s == " \t\n\v\f\r"
  CL_CRLF = 8U,
};

struct ClassTable {
  s::uint8_t cls[256] = {};

  constexpr ClassTable(){
    for (int b = 0; b < 256; ++b){
      s::uint8_t f = 0U;
      if ((b >= 'A' and b <= 'Z') or (b >= 'a' and b <= 'z') or b >= 0x80)
        f |= CL_LETTER;
      if (b >= '0' and b <= '9') f |= CL_DIGIT;
      if (b == ' ' or b == '\t' or b == '\n' or
          b == '\v' or b == '\f' or b == '\r') f |= CL_SPACE;
      if (b == '\r' or b == '\n') f |= CL_CRLF;
      cls[b] = f;
    }
  }
};

constexpr ClassTable kTable{};

inline s::uint8_t class_of(char c){
  return kTable.cls[(unsigned char)c];
}

// The optional prefix of alternative (2): anything that is not CR/LF, a letter
// or a digit -- which includes space and tab, so "\ttab" is one segment.
inline bool is_word_prefix(char c){
  return (class_of(c) & (CL_LETTER | CL_DIGIT | CL_CRLF)) == 0U;
}

// The body of alternative (4): not whitespace, not a letter, not a digit. High
// bytes count as letters, so they are excluded here too.
inline bool is_symbol(char c){
  return (class_of(c) & (CL_LETTER | CL_DIGIT | CL_SPACE)) == 0U;
}

#if defined(TOKENIZER_PRETOKENIZER_NEON)

// Each answers only "are all 16 of these bytes in the class?", which keeps the
// vector path free of lane-index bit twiddling -- one miss and the scalar loop
// takes over.
inline bool all_letters(const char* p){
  const uint8x16_t v = vld1q_u8((const s::uint8_t*)p);
  const uint8x16_t folded = vorrq_u8(v, vdupq_n_u8(0x20U)); // A-Z -> a-z
  const uint8x16_t lo = vcgeq_u8(folded, vdupq_n_u8((s::uint8_t)'a'));
  const uint8x16_t hi = vcleq_u8(folded, vdupq_n_u8((s::uint8_t)'z'));
  const uint8x16_t alpha = vandq_u8(lo, hi);
  const uint8x16_t high = vcgeq_u8(v, vdupq_n_u8(0x80U));
  return vminvq_u8(vorrq_u8(alpha, high)) == 0xFFU;
}

inline bool all_digits(const char* p){
  const uint8x16_t v = vld1q_u8((const s::uint8_t*)p);
  const uint8x16_t shifted = vsubq_u8(v, vdupq_n_u8((s::uint8_t)'0'));
  return vminvq_u8(vcleq_u8(shifted, vdupq_n_u8(9U))) == 0xFFU;
}

// " \t\n\v\f\r" is 0x20 plus the contiguous run 0x09-0x0D.
inline bool all_spaces(const char* p){
  const uint8x16_t v = vld1q_u8((const s::uint8_t*)p);
  const uint8x16_t space = vceqq_u8(v, vdupq_n_u8(0x20U));
  const uint8x16_t shifted = vsubq_u8(v, vdupq_n_u8(0x09U));
  const uint8x16_t ctrl = vcleq_u8(shifted, vdupq_n_u8(0x04U));
  return vminvq_u8(vorrq_u8(space, ctrl)) == 0xFFU;
}

#endif

s::size_t letter_run(const char* p, s::size_t n, s::size_t i){
  const s::size_t start = i;
  // Scalar for the first 16 bytes: prose runs average ~5 bytes, and probing a
  // vector for those costs more than it saves. Only a run ALREADY 16 long is
  // worth continuing in vectors -- code, CJK, long digit strings, base64.
  const s::size_t head = (n - i) < 16U ? n : i + 16U;
  while (i < head and (class_of(p[i]) & CL_LETTER) != 0U) ++i;
  if (i < head or i == n) return i - start;

#if defined(TOKENIZER_PRETOKENIZER_NEON)
  while (i + 16U <= n and all_letters(p + i)) i += 16U;
#endif
  while (i < n and (class_of(p[i]) & CL_LETTER) != 0U) ++i;
  return i - start;
}

s::size_t digit_run(const char* p, s::size_t n, s::size_t i){
  const s::size_t start = i;
  // Scalar for the first 16 bytes: prose runs average ~5 bytes, and probing a
  // vector for those costs more than it saves. Only a run ALREADY 16 long is
  // worth continuing in vectors -- code, CJK, long digit strings, base64.
  const s::size_t head = (n - i) < 16U ? n : i + 16U;
  while (i < head and (class_of(p[i]) & CL_DIGIT) != 0U) ++i;
  if (i < head or i == n) return i - start;

#if defined(TOKENIZER_PRETOKENIZER_NEON)
  while (i + 16U <= n and all_digits(p + i)) i += 16U;
#endif
  while (i < n and (class_of(p[i]) & CL_DIGIT) != 0U) ++i;
  return i - start;
}

s::size_t space_run(const char* p, s::size_t n, s::size_t i){
  const s::size_t start = i;
  // Scalar for the first 16 bytes: prose runs average ~5 bytes, and probing a
  // vector for those costs more than it saves. Only a run ALREADY 16 long is
  // worth continuing in vectors -- code, CJK, long digit strings, base64.
  const s::size_t head = (n - i) < 16U ? n : i + 16U;
  while (i < head and (class_of(p[i]) & CL_SPACE) != 0U) ++i;
  if (i < head or i == n) return i - start;

#if defined(TOKENIZER_PRETOKENIZER_NEON)
  while (i + 16U <= n and all_spaces(p + i)) i += 16U;
#endif
  while (i < n and (class_of(p[i]) & CL_SPACE) != 0U) ++i;
  return i - start;
}

s::size_t symbol_run(const char* p, s::size_t n, s::size_t i){
  const s::size_t start = i;
  while (i < n and is_symbol(p[i])) ++i;
  return i - start;
}

s::size_t crlf_run(const char* p, s::size_t n, s::size_t i){
  const s::size_t start = i;
  while (i < n and (class_of(p[i]) & CL_CRLF) != 0U) ++i;
  return i - start;
}

inline char fold(char c){
  return (char)(c | 0x20);
}

// Alternative (1). The four contraction forms are disjoint after the quote, so
// the order between them does not matter.
s::size_t match_contraction(const char* p, s::size_t n, s::size_t i){
  if (p[i] != '\'' or i + 1U >= n) return 0U;
  const char a = fold(p[i + 1U]);
  if (a == 's' or a == 'd' or a == 'm' or a == 't') return 2U;
  if (i + 2U >= n) return 0U;
  const char b = fold(p[i + 2U]);
  if ((a == 'l' and b == 'l') or (a == 'v' and b == 'e') or
      (a == 'r' and b == 'e')) return 3U;
  return 0U;
}

// Alternative (2). When p[i] can be a prefix it is never itself a letter, so
// the no-prefix retry the greedy `?` would perform can only fail -- hence the
// either/or rather than a fallback.
s::size_t match_word(const char* p, s::size_t n, s::size_t i){
  if (is_word_prefix(p[i])){
    const s::size_t run = letter_run(p, n, i + 1U);
    return run > 0U ? run + 1U : 0U;
  }
  return letter_run(p, n, i);
}

// Alternative (4). A leading space is only taken when a symbol follows it; a
// space is whitespace, so `symbol_run` at the space itself is always 0 and the
// greedy ` ?` needs no explicit give-back.
s::size_t match_symbols(const char* p, s::size_t n, s::size_t i){
  const s::size_t j = (p[i] == ' ') ? i + 1U : i;
  const s::size_t run = symbol_run(p, n, j);
  if (run == 0U) return 0U;
  const s::size_t end = j + run;
  return (end - i) + crlf_run(p, n, end);
}

} // namespace

s::size_t gpt4_segment_length(const char* text, s::size_t size, s::size_t pos){
  if (pos >= size) return 0U;

  if (const s::size_t n = match_contraction(text, size, pos); n > 0U) return n;
  if (const s::size_t n = match_word(text, size, pos); n > 0U) return n;

  if (const s::size_t digits = digit_run(text, size, pos); digits > 0U)
    return digits > 3U ? 3U : digits;

  if (const s::size_t n = match_symbols(text, size, pos); n > 0U) return n;

  const s::size_t spaces = space_run(text, size, pos);
  if (spaces == 0U) return 0U;

  // (5) up to and including the last CR/LF in the run.
  for (s::size_t k = spaces; k > 0U; --k){
    if ((class_of(text[pos + k - 1U]) & CL_CRLF) != 0U) return k;
  }

  // (6) the whole run at end of input, otherwise one character short of it.
  if (pos + spaces == size) return spaces;
  if (spaces > 1U) return spaces - 1U;

  // (7) whatever is left, which at this point is a single space.
  return spaces;
}

s::vector<s::vector<char>> gpt4_segments(const s::vector<char>& text){
  s::vector<s::vector<char>> out;
  const char* p = text.data();
  const s::size_t n = text.size();

  // Roughly one segment per short word; saves the early doubling on big inputs.
  out.reserve(n / 4U + 1U);

  s::size_t i = 0U;
  while (i < n){
    const s::size_t len = gpt4_segment_length(p, n, i);
    if (len == 0U){
      ++i; // unreachable for this pattern; matches regex_iterator's behaviour
      continue;
    }
    out.emplace_back(p + i, p + i + len);
    i += len;
  }
  return out;
}

} // tokenizer
