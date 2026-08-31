#include <bpe_builder.h>

#include <bpe_test_helpers.h>

#include <gtest/gtest.h>

#include <sstream>
#include <string>
#include <vector>

// Unit tests for tokenizer::tokenize and tokenizer::print_token_table -- the
// consumer side of the learned BPE table. Tables are built by hand with
// bpe_test::pack so every case is deterministic (the builder's tie-break is
// not). Several tests are regression guards for bugs the first draft of these
// functions had; each is tagged with the bug it covers.

using bpe_test::apply_merges;
using bpe_test::bytes;
using bpe_test::pack;
using bpe_test::repeated;
using bpe_test::Table;
using tokenizer::build_bpe_table;
using tokenizer::print_token_table;
using tokenizer::tokenize;

namespace {

using Toks = std::vector<tokenizer::CP>;

std::string print_table(const Table& table) {
  std::ostringstream out;
  print_token_table(out, table);
  return out.str();
}

// ---- tokenize: behaviour ----

TEST(TokenizeTest, EmptyTextYieldsNoTokens) {
  EXPECT_TRUE(tokenize(bytes(""), Table{}).empty());
}

TEST(TokenizeTest, PassesRawBytesThroughWhenTableEmpty) {
  EXPECT_EQ(tokenize(bytes("abc"), Table{}), (Toks{'a', 'b', 'c'}));
}

TEST(TokenizeTest, AppliesASingleMerge) {
  const Table table{{pack('a', 'b'), 256}};
  EXPECT_EQ(tokenize(bytes("ab"), table), (Toks{256}));
  EXPECT_EQ(tokenize(bytes("xabx"), table), (Toks{'x', 256, 'x'}));
}

TEST(TokenizeTest, ChainsLeftGrowingMerges) {
  const Table table{{pack('a', 'a'), 256}, {pack(256, 'a'), 257}};
  EXPECT_EQ(tokenize(bytes("aaa"), table), (Toks{257}));
}

TEST(TokenizeTest, MatchesReferenceEncoderOnLearnedTable) {
  // The textbook BPE corpus (see bpe_pipeline_test.cpp): low x5, lower x2,
  // newest x6, widest x3. tokenize must agree with the reference encoder for
  // every input, in and out of vocabulary.
  const auto table = build_bpe_table(
      repeated({"low", "lower", "newest", "widest"}, {5, 2, 6, 3}), 12);

  for (const char* w :
       {"low", "lower", "newest", "widest", "lowest", "slowest", "", "zzz"}) {
    EXPECT_EQ(tokenize(bytes(w), table), apply_merges(w, table)) << "word: " << w;
  }
}

// ---- tokenize: regression guards ----

// BUG B1: (CP)text[i] / (MK)text[i] sign-extended a signed char, so byte 0x80
// became 0xFFFFFF80 -- no merge key matched and raw tokens came out as ~2^32.
TEST(TokenizeTest, DoesNotSignExtendHighBytes) {
  EXPECT_EQ(tokenize(bytes(std::string("\x80", 1)), Table{}), (Toks{0x80u}));

  const std::string ee("\xC3\xA9", 2);  // one 'e'-acute, bytes C3 A9
  const Table table{{pack(0xC3u, 0xA9u), 256}};
  EXPECT_EQ(tokenize(bytes(ee), table), (Toks{256}));
}

// BUG B2: a single left-to-right pass applied whichever pair came first by
// position, ignoring merge rank. (b,c) outranks (a,b) but starts one byte
// later; the old code produced {257, 'c'} instead of {'a', 256}.
TEST(TokenizeTest, RespectsMergeRankNotPosition) {
  const Table table{{pack('b', 'c'), 256}, {pack('a', 'b'), 257}};
  EXPECT_EQ(tokenize(bytes("abc"), table), apply_merges("abc", table));
  EXPECT_EQ(tokenize(bytes("abc"), table), (Toks{'a', 256}));
}

// BUG B2: after forming a token the old code never re-checked it against its
// left neighbour, so (c,256) was missed. Old result: {'c', 256}.
TEST(TokenizeTest, ReScansLeftNeighbourAfterMerge) {
  const Table table{{pack('a', 'b'), 256}, {pack('c', 256), 257}};
  EXPECT_EQ(tokenize(bytes("cab"), table), apply_merges("cab", table));
  EXPECT_EQ(tokenize(bytes("cab"), table), (Toks{257}));
}

// ---- print_token_table ----

TEST(PrintTokenTableTest, FormatsASingleMerge) {
  const Table table{{pack('a', 'b'), 256}};
  EXPECT_EQ(print_table(table), "a + b = 256\n");
}

// BUG C: the old code iterated merges ordered by packed pair key, not by rank.
// pack(256,' ') < pack('a','a'), so (256,' ')->257 printed first -- before
// legend[256] existed -- giving a blank left side and reversed lines.
TEST(PrintTokenTableTest, ExpandsHigherOrderMergesInRankOrder) {
  const Table table{{pack('a', 'a'), 256}, {pack(256, ' '), 257}};
  EXPECT_EQ(print_table(table), "a + a = 256\naa +   = 257\n");
}

// BUG D: raw bytes went straight to the stream. Non-printable / non-ASCII bytes
// must render as \xNN (lowercase).
TEST(PrintTokenTableTest, EscapesNonPrintableAndHighBytes) {
  const Table table{{pack(0xC3u, 0xA9u), 256}};
  EXPECT_EQ(print_table(table), "\\xc3 + \\xa9 = 256\n");
}

TEST(PrintTokenTableTest, KeepsNulBytesAsHexEscape) {
  const Table table{{pack('a', 0u), 256}};
  EXPECT_EQ(print_table(table), "a + \\x00 = 256\n");
}

TEST(PrintTokenTableTest, ExpandsEscapedHigherOrderToken) {
  const Table table{{pack(0xC3u, 0xA9u), 256}, {pack(256, '!'), 257}};
  EXPECT_EQ(print_table(table), "\\xc3 + \\xa9 = 256\n\\xc3\\xa9 + ! = 257\n");
}

}  // namespace
