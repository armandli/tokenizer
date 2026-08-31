#include <bpe_builder.h>

#include <bpe_test_helpers.h>

#include <gtest/gtest.h>

#include <string>
#include <vector>

// Tests for the GPT-4 pre-tokenizer translation (tokenizer::gpt4_presplit).
//
// Reference: tiktoken's cl100k_base pattern (Rust `fancy-regex` engine):
//   '(?i:[sdmt]|ll|ve|re)|[^\r\n\p{L}\p{N}]?+\p{L}+|\p{N}{1,3}
//   | ?[^\s\p{L}\p{N}]++[\r\n]*|\s*[\r\n]|\s+(?!\S)|\s+
// std::regex ECMAScript can't do (?i:...), possessive quantifiers, or
// \p{L}/\p{N}; see bpe_builder.cpp for how each maps. The ascii_cases()
// expectations were taken from that literal pattern via the Python `regex`
// module and checked byte-for-byte against gpt4_presplit over 300k+ pure-ASCII
// fuzz strings. non_ascii_cases() pins the approximate UTF-8 behaviour (non-ASCII
// letters/digits are faithful; non-ASCII punctuation / non-decimal digits are
// grouped as letters).
//
// Behaviour to note (differs from GPT-2): contractions are case-insensitive; a
// word takes any single non-CR/LF non-alnum prefix, not just a space; digits
// split into groups of <=3 with no leading space; newline runs are their own
// token.

using bpe_test::bytes;
using tokenizer::gpt4_presplit;

namespace {

std::vector<std::string> to_strings(const std::vector<std::vector<char>>& segs) {
  std::vector<std::string> out;
  for (const auto& s : segs) out.emplace_back(s.begin(), s.end());
  return out;
}

std::vector<std::string> presplit(const std::string& in) {
  return to_strings(gpt4_presplit(bytes(in)));
}

struct Case {
  const char* in;
  std::vector<std::string> want;
};

// --- ASCII: asserted byte-identical to the literal cl100k pattern -----------
const std::vector<Case>& ascii_cases() {
  static const std::vector<Case> cases = {
      {"Hello world", {"Hello", " world"}},
      {"Hello  world", {"Hello", " ", " world"}},
      {"Hello   world", {"Hello", "  ", " world"}},
      {"I've don't they're",
       {"I", "'ve", " don", "'t", " they", "'re"}},
      {"IT'S WHAT I'D DO",
       {"IT", "'S", " WHAT", " I", "'D", " DO"}},
      {"'s 'S 'LL 've 'RE",
       {"'s", " '", "S", " '", "LL", " '", "ve", " '", "RE"}},
      {"'x", {"'x"}},
      {"a'b", {"a", "'b"}},
      {"won't've", {"won", "'t", "'ve"}},
      {"(parenthesised) [bracketed] {braced}",
       {"(parenthesised", ")", " [", "bracketed", "]", " {", "braced", "}"}},
      {"#hashtag @mention", {"#hashtag", " @", "mention"}},
      {"it's 3 cats and 42 dogs",
       {"it", "'s", " ", "3", " cats", " and", " ", "42", " dogs"}},
      {"1234567", {"123", "456", "7"}},
      {"1 22 333 4444 55555",
       {"1", " ", "22", " ", "333", " ", "444", "4", " ", "555", "55"}},
      {"$100 and 3.50", {"$", "100", " and", " ", "3", ".", "50"}},
      {" 123 456", {" ", "123", " ", "456"}},
      {"v2.0.1", {"v", "2", ".", "0", ".", "1"}},
      {"line\nbreak", {"line", "\n", "break"}},
      {"a\n\nb", {"a", "\n\n", "b"}},
      {"a\n\n\n\nb", {"a", "\n\n\n\n", "b"}},
      {"para1\r\npara2", {"para", "1", "\r\n", "para", "2"}},
      {"trailing\n", {"trailing", "\n"}},
      {"dots...\n\nmore", {"dots", "...\n\n", "more"}},
      {"punct!!!\n x", {"punct", "!!!\n", " x"}},
      {"  leading", {" ", " leading"}},
      {"trailing   ", {"trailing", "   "}},
      {"a   b   c", {"a", "  ", " b", "  ", " c"}},
      {"tab\ttab", {"tab", "\ttab"}},
      {"mix \t\n \t run", {"mix", " \t\n", " \t", " run"}},
      {"", {}},
      {" ", {" "}},
      {"\n", {"\n"}},
      {"\n\n", {"\n\n"}},
      {"   ", {"   "}},
      {"under_score CamelCase", {"under", "_score", " CamelCase"}},
      {"https://a.b/c?d=1",
       {"https", "://", "a", ".b", "/c", "?d", "=", "1"}},
      // 0x1c-0x1f are punctuation content (Rust \s == \p{White_Space}).
      {"x\x1c\x1cy", {"x", "\x1c\x1c", "y"}},
      {"a.\x1f" "b", {"a", ".\x1f", "b"}},
  };
  return cases;
}

// --- non-ASCII: characterization only --------------------------------------
const std::vector<Case>& non_ascii_cases() {
  static const std::vector<Case> cases = {
      {"caf\xc3\xa9", {"caf\xc3\xa9"}},
      {"R\xc3\x89SUM\xc3\x89 caf\xc3\xa9", {"R\xc3\x89SUM\xc3\x89", " caf\xc3\xa9"}},
      {"na\xc3\xafve", {"na\xc3\xafve"}},
      {"\xe4\xbd\xa0\xe5\xa5\xbd \xe4\xb8\x96\xe7\x95\x8c",  // "你好 世界"
       {"\xe4\xbd\xa0\xe5\xa5\xbd", " \xe4\xb8\x96\xe7\x95\x8c"}},
      {"\xd0\x9f\xd1\x80\xd0\xb8\xd0\xb2\xd0\xb5\xd1\x82 \xd0\xbc\xd0\xb8\xd1\x80",  // "Привет мир"
       {"\xd0\x9f\xd1\x80\xd0\xb8\xd0\xb2\xd0\xb5\xd1\x82",
        " \xd0\xbc\xd0\xb8\xd1\x80"}},
      {"l'\xc3\xa9t\xc3\xa9 \xc3\xa0 Paris",  // "l'été à Paris"
       {"l", "'\xc3\xa9t\xc3\xa9", " \xc3\xa0", " Paris"}},
      {"IT'S d\xc3\xa9j\xc3\xa0 vu",  // "IT'S déjà vu"
       {"IT", "'S", " d\xc3\xa9j\xc3\xa0", " vu"}},
      {"emoji \xf0\x9f\x98\x80 y", {"emoji", " \xf0\x9f\x98\x80", " y"}},
      // KNOWN GAPS vs true cl100k:
      //   "a。b"  -> cl100k {"a", "。b"}   (。 is a punctuation word-prefix)
      //   "1²³"   -> cl100k {"1²³"}        (superscripts are \p{No})
      {"a\xe3\x80\x82" "b", {"a\xe3\x80\x82" "b"}},
      {"1\xc2\xb2\xc2\xb3", {"1", "\xc2\xb2\xc2\xb3"}},
      {"3\xc2\xbd cups", {"3", "\xc2\xbd", " cups"}},
  };
  return cases;
}

TEST(Gpt4PatternTest, MatchesCanonicalCl100kSplitOnAscii) {
  for (const auto& c : ascii_cases()) {
    EXPECT_EQ(presplit(c.in), c.want)
        << "input: " << ::testing::PrintToString(std::string(c.in));
  }
}

TEST(Gpt4PatternTest, NonAsciiCharacterization) {
  for (const auto& c : non_ascii_cases()) {
    EXPECT_EQ(presplit(c.in), c.want)
        << "input: " << ::testing::PrintToString(std::string(c.in));
  }
}

TEST(Gpt4PatternTest, ContractionsAreCaseInsensitive) {
  EXPECT_EQ(presplit("you're"), (std::vector<std::string>{"you", "'re"}));
  EXPECT_EQ(presplit("YOU'RE"), (std::vector<std::string>{"YOU", "'RE"}));
  EXPECT_EQ(presplit("It'Ll"), (std::vector<std::string>{"It", "'Ll"}));
}

TEST(Gpt4PatternTest, DigitsSplitIntoGroupsOfAtMostThree) {
  EXPECT_EQ(presplit("1234567890"),
            (std::vector<std::string>{"123", "456", "789", "0"}));
  // no leading space is attached to a digit run (and it still splits by 3s)
  EXPECT_EQ(presplit(" 2024"), (std::vector<std::string>{" ", "202", "4"}));
}

TEST(Gpt4PatternTest, TreatsNulByteAsOrdinaryContent) {
  const std::vector<char> text{'a', '\0', '\0', 'b'};
  EXPECT_EQ(to_strings(gpt4_presplit(text)),
            (std::vector<std::string>{"a", std::string("\0\0", 2), "b"}));
}

TEST(Gpt4PatternTest, BuildGpt4TableLearnsMergesFromRepeatedCorpus) {
  std::vector<char> corpus;
  for (int i = 0; i < 20; ++i) {
    const std::string chunk = "the cat sat on the mat\n";
    corpus.insert(corpus.end(), chunk.begin(), chunk.end());
  }

  const auto table = tokenizer::build_gpt4_table(corpus, 10);

  ASSERT_FALSE(table.empty());
  EXPECT_LE(table.size(), 10u);
  for (const auto& [key, id] : table) {
    (void)key;
    EXPECT_GE(id, 256u);
    EXPECT_LT(id, 256u + table.size());
  }
}

} // namespace
