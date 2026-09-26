#include <pretokenizer.h>

#include <bpe_builder.h>
#include <bpe_internal.h>

#include <gtest/gtest.h>

#include <random>
#include <string>
#include <vector>

// The hand-written scanner replaces std::regex in gpt4_presplit, so the binding
// requirement is not "does it look right" but "is it byte-identical to the
// regex it replaced". gpt4_presplit_regex is kept compiled for exactly that,
// and almost everything here is a differential check against it.
//
// gpt4_pattern_test.cpp still asserts the pinned cl100k expectations through
// gpt4_presplit, so that file now covers the scanner too.

using tokenizer::gpt4_presplit_regex;
using tokenizer::gpt4_segments;

namespace {

std::vector<char> bytes_of(const std::string& s) {
  return {s.begin(), s.end()};
}

std::vector<std::string> to_strings(
    const std::vector<std::vector<char>>& segs) {
  std::vector<std::string> out;
  for (const auto& s : segs) out.emplace_back(s.begin(), s.end());
  return out;
}

// The segments must be identical AND must reassemble into the input -- a
// scanner that loses or duplicates bytes could otherwise agree on a prefix.
void expect_matches_regex(const std::vector<char>& text) {
  const auto want = to_strings(gpt4_presplit_regex(text));
  const auto got = to_strings(gpt4_segments(text));
  const std::string printable(text.begin(), text.end());
  ASSERT_EQ(got, want) << "input: " << ::testing::PrintToString(printable);

  std::string rejoined;
  for (const auto& segment : got) rejoined += segment;
  EXPECT_EQ(rejoined, printable);
}

void expect_matches_regex(const std::string& text) {
  expect_matches_regex(bytes_of(text));
}

} // namespace

TEST(PretokenizerTest, MatchesRegexOnWhitespaceRunBoundaries) {
  // Alternative (6) `\s+(?!\S)` and (5) `\s*[\r\n]` are where a scanner is most
  // likely to be subtly wrong: whether a run gives its last character to the
  // next word, and where a run containing a newline is cut.
  for (const char* in : {"", " ", "  ", "   ", "    ", "a b", "a  b", "a   b", "a    b",
                         " a", "  a", "   a", "a ", "a  ", "a   ",
                         "\n", "\n\n", "\r", "\r\n", "\r\r", "\n\r", "\r\n\r\n",
                         "a\nb", "a\n\nb", "a\r\nb", "a \n b", "a \n\n b",
                         "a\t\nb", "a\n\tb", "a \t\n \t b", "a\v\fb", "a \v \f \n \r b",
                         "  \n  ", "\n  \n", " \n", "\n ", "  \n", "\n  ",
                         "x\n\n\n\n\ny", "x     \n     y"}) {
    expect_matches_regex(std::string(in));
  }
}

TEST(PretokenizerTest, MatchesRegexOnContractionsAndWords) {
  for (const char* in : {"'", "''", "'''", "'s", "'S", "'x", "'1", "' ", "'\n",
                         "'l", "'ll", "'LL", "'lL", "'llx", "'v", "'ve", "'VE", "'r", "'re",
                         "a'", "a's", "a'b", "won't've", "y'all'd've",
                         "_a", ".a", "-a", "$a", "(a", "\ta", " a", "a_b", "a.b",
                         "CamelCase", "under_score", "MiXeD"}) {
    expect_matches_regex(std::string(in));
  }
}

TEST(PretokenizerTest, MatchesRegexOnDigitsAndSymbols) {
  for (const char* in : {"0", "1", "12", "123", "1234", "12345", "123456", "1234567",
                         "1.2", "1,234", "1e10", "0x1f", "v2.0.1", "$100", "3.50",
                         "-1", " 1", " 12", " 123", " 1234", "a1", "1a", "a1b2c3",
                         "!", "!!", "!!!", "!!!\n", "!!!\n\n", " !", " !!", "...", "...\n",
                         "://", "?a=1&b=2", "<<>>", "\x1c", "\x1c\x1c", "a\x1f" "b"}) {
    expect_matches_regex(std::string(in));
  }
}

TEST(PretokenizerTest, MatchesRegexOnNulAndHighBytes) {
  expect_matches_regex(std::vector<char>{'a', '\0', '\0', 'b'});
  expect_matches_regex(std::vector<char>{'\0'});
  expect_matches_regex(std::vector<char>{'\0', ' ', '\0'});
  expect_matches_regex(std::vector<char>{'a', '\0', '1', '\0', ' '});

  for (const char* in : {"caf\xc3\xa9", "na\xc3\xafve", "\xe4\xbd\xa0\xe5\xa5\xbd",
                         "l'\xc3\xa9t\xc3\xa9", "emoji \xf0\x9f\x98\x80 y",
                         "1\xc2\xb2\xc2\xb3", "\xff", "\x80", "\xff\xff", "a\xff" "b",
                         " \xff", ".\xff", "\xff" "1"}) {
    expect_matches_regex(std::string(in));
  }
}

TEST(PretokenizerTest, MatchesRegexOnLongRunsThatExerciseTheVectorPath) {
  // The NEON helpers only engage past 16 bytes, so short inputs never reach
  // them. These do, including runs that end mid-vector.
  for (std::size_t len = 1; len <= 70; ++len) {
    expect_matches_regex(std::string(len, 'a'));
    expect_matches_regex(std::string(len, '7'));
    expect_matches_regex(std::string(len, ' '));
    expect_matches_regex(std::string(len, '\n'));
    expect_matches_regex(std::string(len, '.'));
    expect_matches_regex(std::string(len, '\xc3'));
    expect_matches_regex("x" + std::string(len, 'a') + "1");
    expect_matches_regex("x" + std::string(len, ' ') + "y");
    expect_matches_regex(std::string(len, ' ') + "\n" + std::string(len, ' '));
  }
}

TEST(PretokenizerTest, MatchesRegexOnRandomFullByteStrings) {
  // Unrestricted bytes: the only way to catch a class-table or ordering mistake
  // on input nobody thought to write down.
  std::mt19937 rng(20260926U);
  std::uniform_int_distribution<int> byte(0, 255);
  std::uniform_int_distribution<std::size_t> length(0U, 64U);

  for (int trial = 0; trial < 20000; ++trial) {
    std::vector<char> text(length(rng));
    for (char& c : text) c = (char)(unsigned char)byte(rng);
    expect_matches_regex(text);
    if (::testing::Test::HasFatalFailure()) return;
  }
}

TEST(PretokenizerTest, MatchesRegexOnRandomTextLikeStrings) {
  // Random bytes are mostly high bytes and symbols, which under-exercises the
  // word/space/digit interplay that dominates real corpora. This alphabet is
  // weighted the other way.
  static const std::string alphabet =
      "aaaabbbcccdddeeeefffggghhhiiiijjkkllllmmmnnnnoooopppqrrrrssssttttuuuvvwwxyyz"
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789      \t\n\n\r\v\f'''...,,,!?-_$#@/:;()[]{}"
      "\xc3\xa9\xc3\xa0\xe4\xbd\xa0\xf0\x9f\x98\x80";

  std::mt19937 rng(20260927U);
  std::uniform_int_distribution<std::size_t> pick(0U, alphabet.size() - 1U);
  std::uniform_int_distribution<std::size_t> length(0U, 200U);

  for (int trial = 0; trial < 20000; ++trial) {
    std::string text(length(rng), '\0');
    for (char& c : text) c = alphabet[pick(rng)];
    expect_matches_regex(text);
    if (::testing::Test::HasFatalFailure()) return;
  }
}

TEST(PretokenizerTest, MatchesRegexOnProseParagraph) {
  const std::string prose =
      " = Valkyria Chronicles III = \n\n Senjou no Valkyria 3 : Unrecorded Chronicles "
      "( Japanese : \xe6\x88\xa6\xe5\xa0\xb4\xe3\x81\xae\xe3\x83\xaf\xe3\x83\xab, lit. "
      "Valkyria of the Battlefield 3 ) , commonly referred to as Valkyria Chronicles III "
      "outside Japan , is a tactical role @-@ playing video game developed by Sega and "
      "Media.Vision for the PlayStation Portable . Released in January 2011 in Japan , it "
      "is the third game in the Valkyria series . Employing the same fusion of tactical "
      "and real @-@ time gameplay as its predecessors , the story runs parallel to the "
      "first game and follows the \" Nameless \" , a penal military unit serving the "
      "nation of Gallia during the Second Europan War who perform secret black "
      "operations and are pitted against the Imperial unit \" Calamaty Raven \" . \n";

  expect_matches_regex(prose);

  // Repeated, so the long-run vector paths see realistic prose too.
  std::string repeated;
  for (int i = 0; i < 20; ++i) repeated += prose;
  expect_matches_regex(repeated);
}
