#include <bpe_builder.h>

#include <bpe_test_helpers.h>

#include <gtest/gtest.h>

#include <string>
#include <vector>

// Unit tests for tokenizer::build_decode_map and tokenizer::decode -- the
// inverse of tokenize. Tables are hand-built with bpe_test::pack so every case
// is deterministic. DecodeMapTest.SeedsBaseAlphabet is a regression guard for
// the fix that adds bytes 0..255 to the map (without it decode() errors on any
// raw-byte token, i.e. almost every real stream).

using bpe_test::bytes;
using bpe_test::pack;
using bpe_test::repeated;
using bpe_test::Table;
using tokenizer::build_bpe_table;
using tokenizer::build_decode_map;
using tokenizer::decode;
using tokenizer::tokenize;

namespace {

using Toks = std::vector<tokenizer::CP>;
using Bytes = std::vector<char>;

// Build the decode map or fail the test.
tokenizer::DecodeMap decode_map(const Table& table) {
  auto m = build_decode_map(table);
  EXPECT_TRUE(m.has_value());
  return m.value_or(tokenizer::DecodeMap{});
}

// Decode or fail the test.
Bytes dec(const Toks& toks, const tokenizer::DecodeMap& m) {
  auto out = decode(toks, m);
  EXPECT_TRUE(out.has_value());
  return out.value_or(Bytes{});
}

// ---- build_decode_map ----

TEST(DecodeMapTest, SeedsBaseAlphabetEvenForEmptyTable) {
  const auto m = decode_map(Table{});
  EXPECT_EQ(m.size(), 256u);
  EXPECT_EQ(m.at(0x41u), (Bytes{'A'}));
  EXPECT_EQ(m.at(0x00u), (Bytes{'\0'}));
  EXPECT_EQ(m.at(0x80u), (Bytes{(char)0x80}));
  EXPECT_EQ(m.at(0xFFu), (Bytes{(char)0xFF}));
}

TEST(DecodeMapTest, ExpandsMergedIdsToRawBytes) {
  const Table table{{pack('a', 'b'), 256}, {pack(256, 'c'), 257}};
  const auto m = decode_map(table);

  EXPECT_EQ(m.at(256u), bytes("ab"));
  EXPECT_EQ(m.at(257u), bytes("abc"));
}

// ---- decode: behaviour ----

TEST(DecodeTest, EmptyTokensYieldNoBytes) {
  EXPECT_TRUE(dec(Toks{}, decode_map(Table{})).empty());
}

TEST(DecodeTest, PassesRawByteTokensThrough) {
  EXPECT_EQ(dec(Toks{'h', 'i'}, decode_map(Table{})), bytes("hi"));
}

TEST(DecodeTest, ExpandsASingleMerge) {
  const Table table{{pack('a', 'b'), 256}};
  const auto m = decode_map(table);
  EXPECT_EQ(dec(Toks{256}, m), bytes("ab"));
  EXPECT_EQ(dec(Toks{'x', 256, 'x'}, m), bytes("xabx"));
}

TEST(DecodeTest, ExpandsNestedMerges) {
  const Table table{{pack('a', 'a'), 256}, {pack(256, 'a'), 257}};
  EXPECT_EQ(dec(Toks{257}, decode_map(table)), bytes("aaa"));
}

TEST(DecodeTest, PreservesHighBytesWithoutSignExtension) {
  const Table table{{pack(0xC3u, 0xA9u), 256}};  // one 'e'-acute, bytes C3 A9
  EXPECT_EQ(dec(Toks{256}, decode_map(table)),
            (Bytes{(char)0xC3, (char)0xA9}));
}

TEST(DecodeTest, RejectsOutOfVocabToken) {
  const Table table{{pack('a', 'b'), 256}};
  const auto out = decode(Toks{9999}, decode_map(table));
  ASSERT_FALSE(out.has_value());
  EXPECT_NE(std::string(out.error().what()).find("9999"), std::string::npos);
}

// ---- decode: round trip against tokenize ----

TEST(DecodeTest, RoundTripsEncodedTextOnLearnedTable) {
  // Same textbook corpus as the tokenize / pipeline tests.
  const auto table = build_bpe_table(
      repeated({"low", "lower", "newest", "widest"}, {5, 2, 6, 3}), 12);
  const auto m = decode_map(table);

  for (const char* w :
       {"low", "lower", "newest", "widest", "lowest", "slowest", "", "zzz"}) {
    EXPECT_EQ(dec(tokenize(bytes(w), table), m), bytes(w)) << "word: " << w;
  }
}

}  // namespace
