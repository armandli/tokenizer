#include "tokenizer.h"

#include <gtest/gtest.h>

TEST(TokenizerTest, EmptyStringYieldsNoTokens) {
    tokenizer::Tokenizer tok;
    EXPECT_TRUE(tok.tokenize("").empty());
}

TEST(TokenizerTest, SplitsOnWhitespace) {
    tokenizer::Tokenizer tok;
    auto tokens = tok.tokenize("hello world  from   tokenizer");
    ASSERT_EQ(tokens.size(), 4u);
    EXPECT_EQ(tokens[0], "hello");
    EXPECT_EQ(tokens[1], "world");
    EXPECT_EQ(tokens[2], "from");
    EXPECT_EQ(tokens[3], "tokenizer");
}

TEST(TokenizerTest, TrimsLeadingAndTrailingWhitespace) {
    tokenizer::Tokenizer tok;
    auto tokens = tok.tokenize("  padded  ");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0], "padded");
}
