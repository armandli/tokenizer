#include <bpe_builder.h>

#include <bpe_test_helpers.h>

#include <gtest/gtest.h>

#include <string>
#include <vector>

// Tests for the GPT-2 pre-tokenizer translation (tokenizer::gpt2_presplit).
//
// Reference: OpenAI's GPT-2 pattern, as used by encoder.py (`import regex as re`
// -- the third-party `regex` module, whose \s == \p{White_Space}, NOT Python's
// builtin `re`):
//   's|'t|'re|'ve|'m|'ll|'d| ?\p{L}+| ?\p{N}+| ?[^\s\p{L}\p{N}]+|\s+(?!\S)|\s+
// std::regex ECMAScript has no \p{L}/\p{N}, so gpt2_presplit is a byte-level
// translation (see bpe_builder.cpp). The ascii_cases() expectations were taken
// from that literal pattern via the `regex` module and checked byte-for-byte
// against gpt2_presplit over 300k+ pure-ASCII fuzz strings. For UTF-8 it is
// faithful for letters/digits but groups non-ASCII punctuation as letters --
// non_ascii_cases() pins that approximate behaviour.

using bpe_test::bytes;
using tokenizer::gpt2_presplit;

namespace {

std::vector<std::string> to_strings(const std::vector<std::vector<char>>& segs) {
  std::vector<std::string> out;
  for (const auto& s : segs) out.emplace_back(s.begin(), s.end());
  return out;
}

std::vector<std::string> presplit(const std::string& in) {
  return to_strings(gpt2_presplit(bytes(in)));
}

struct Case {
  const char* in;
  std::vector<std::string> want;
};

// --- ASCII: asserted byte-identical to the canonical GPT-2 pattern ----------
const std::vector<Case>& ascii_cases() {
  static const std::vector<Case> cases = {
      {"Hello world", {"Hello", " world"}},
      {"Hello  world", {"Hello", " ", " world"}},
      {"Hello   world", {"Hello", "  ", " world"}},
      {"Hello    world!", {"Hello", "   ", " world", "!"}},
      {"  leading", {" ", " leading"}},
      {"trailing   ", {"trailing", "   "}},
      {"a   b", {"a", "  ", " b"}},
      {"x  y  z", {"x", " ", " y", " ", " z"}},
      {"I've don't they're we'll he'd I'm it's",
       {"I", "'ve", " don", "'t", " they", "'re", " we", "'ll", " he", "'d",
        " I", "'m", " it", "'s"}},
      {"DON'T THEY'RE", {"DON", "'", "T", " THEY", "'", "RE"}},
      {"a'b", {"a", "'", "b"}},
      {"'x", {"'", "x"}},
      {"won't", {"won", "'t"}},
      {"y'all", {"y", "'", "all"}},
      {"it's 3 cats and 42 dogs",
       {"it", "'s", " 3", " cats", " and", " 42", " dogs"}},
      {"foo123bar456", {"foo", "123", "bar", "456"}},
      {"3.14 and 1,000", {"3", ".", "14", " and", " 1", ",", "000"}},
      {"x=y+z", {"x", "=", "y", "+", "z"}},
      {"C++ C#", {"C", "++", " C", "#"}},
      {"https://a.b/c?d=1&e=2",
       {"https", "://", "a", ".", "b", "/", "c", "?", "d", "=", "1", "&", "e",
        "=", "2"}},
      {"email@host.com, ok?",
       {"email", "@", "host", ".", "com", ",", " ok", "?"}},
      {"!!!???...", {"!!!???..."}},
      {"tab\tsep\there", {"tab", "\t", "sep", "\t", "here"}},
      {"line\n\nbreak", {"line", "\n", "\n", "break"}},
      {"a \t \n b   c", {"a", " \t \n", " b", "  ", " c"}},
      {"", {}},
      {" ", {" "}},
      {"\n", {"\n"}},
      {"   ", {"   "}},
      {"a", {"a"}},
      {"\t", {"\t"}},
      {"MixedCase WORDS lower", {"MixedCase", " WORDS", " lower"}},
      {"under_score and-dash", {"under", "_", "score", " and", "-", "dash"}},
      {"end.", {"end", "."}},
      // ASCII FS/GS/RS/US (0x1c-0x1f) are punctuation CONTENT, not whitespace:
      // the `regex` module's \s (like tiktoken's) is \p{White_Space}, which
      // excludes them. They join adjacent punctuation runs and \s+ skips them.
      {"x\x1c\x1cy", {"x", "\x1c\x1c", "y"}},
      {"a.\x1f" "b", {"a", ".\x1f", "b"}},
      {"field\x1f" "rec", {"field", "\x1f", "rec"}},
      {"= \x1d done", {"=", " \x1d", " done"}},
  };
  return cases;
}

// --- non-ASCII: characterization only (see the known-limitation note) -------
const std::vector<Case>& non_ascii_cases() {
  static const std::vector<Case> cases = {
      // Non-ASCII letters group like GPT-2 (bytes shown; "café", "RÉSUMÉ", ...).
      {"caf\xc3\xa9", {"caf\xc3\xa9"}},
      {"R\xc3\x89SUM\xc3\x89", {"R\xc3\x89SUM\xc3\x89"}},
      {"na\xc3\xafve caf\xc3\xa9", {"na\xc3\xafve", " caf\xc3\xa9"}},
      {"\xe4\xbd\xa0\xe5\xa5\xbd \xe4\xb8\x96\xe7\x95\x8c",  // "你好 世界"
       {"\xe4\xbd\xa0\xe5\xa5\xbd", " \xe4\xb8\x96\xe7\x95\x8c"}},
      {"\xd0\x9f\xd1\x80\xd0\xb8\xd0\xb2\xd0\xb5\xd1\x82 \xd0\xbc\xd0\xb8\xd1\x80",  // "Привет мир"
       {"\xd0\x9f\xd1\x80\xd0\xb8\xd0\xb2\xd0\xb5\xd1\x82",
        " \xd0\xbc\xd0\xb8\xd1\x80"}},
      {"l'\xc3\xa9t\xc3\xa9 \xc3\xa0 Paris",  // "l'été à Paris"
       {"l", "'", "\xc3\xa9t\xc3\xa9", " \xc3\xa0", " Paris"}},
      {"emoji \xf0\x9f\x98\x80 ok", {"emoji", " \xf0\x9f\x98\x80", " ok"}},
      {"\xd9\xa4\xd9\xa5 digits", {"\xd9\xa4\xd9\xa5", " digits"}},
      // KNOWN GAP: non-ASCII punctuation is grouped with adjacent letters.
      // True GPT-2 would give {"a", "\xe3\x80\x82", "b"}.
      {"a\xe3\x80\x82" "b", {"a\xe3\x80\x82" "b"}},
  };
  return cases;
}

TEST(Gpt2PatternTest, MatchesCanonicalGpt2SplitOnAscii) {
  for (const auto& c : ascii_cases()) {
    EXPECT_EQ(presplit(c.in), c.want)
        << "input: " << ::testing::PrintToString(std::string(c.in));
  }
}

TEST(Gpt2PatternTest, NonAsciiCharacterization) {
  for (const auto& c : non_ascii_cases()) {
    EXPECT_EQ(presplit(c.in), c.want)
        << "input: " << ::testing::PrintToString(std::string(c.in));
  }
}

TEST(Gpt2PatternTest, TreatsNulByteAsOrdinaryContent) {
  // 0x00 is not whitespace/letter/digit, so it is punctuation content and never
  // terminates a segment.
  const std::vector<char> text{' ', 'a', '\0', '\0', '\0'};
  EXPECT_EQ(to_strings(gpt2_presplit(text)),
            (std::vector<std::string>{" a", std::string("\0\0\0", 3)}));
}

TEST(Gpt2PatternTest, WhitespaceRunKeepsOneSpaceForTheNextWord) {
  // The \s+(?!\S) alternative: a run of N spaces before a word yields (N-1)
  // spaces, then the word with its own leading space. A trailing run keeps all.
  EXPECT_EQ(presplit("a     b"),
            (std::vector<std::string>{"a", "    ", " b"}));
  EXPECT_EQ(presplit("a     "), (std::vector<std::string>{"a", "     "}));
}

TEST(Gpt2PatternTest, BuildGpt2TableLearnsMergesFromRepeatedCorpus) {
  std::vector<char> corpus;
  for (int i = 0; i < 20; ++i) {
    const std::string chunk = "the cat sat on the mat ";
    corpus.insert(corpus.end(), chunk.begin(), chunk.end());
  }

  const auto table = tokenizer::build_gpt2_table(corpus, 10);

  ASSERT_FALSE(table.empty());
  EXPECT_LE(table.size(), 10u);
  for (const auto& [key, id] : table) {
    (void)key;
    EXPECT_GE(id, 256u);
    EXPECT_LT(id, 256u + table.size());
  }
}

} // namespace
