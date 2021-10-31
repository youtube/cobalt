// Copyright 2016 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/base/token.h"

#include <algorithm>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

#include "base/logging.h"
#include "starboard/common/string.h"

namespace base {
namespace {

TEST(TokenTest, Create) {
  std::string kTestString = "test";

  Token empty;
  EXPECT_EQ(empty, "");

  Token non_empty(kTestString);
  EXPECT_EQ(non_empty, kTestString);
}

TEST(TokenTest, CompareEqual) {
  std::string kTestString = "test";

  Token empty_1;
  Token empty_2("");
  EXPECT_EQ(empty_1, empty_2);

  Token non_empty_1(kTestString);
  Token non_empty_2(kTestString);
  EXPECT_EQ(non_empty_1, non_empty_2);
}

TEST(TokenTest, CompareUnequal) {
  std::string kTestString = "test";

  Token empty;
  Token non_empty(kTestString);
  EXPECT_NE(empty, non_empty);
}

TEST(TokenTest, CompareSorted) {
  std::vector<Token> tokens;

  // Tokens constructed in no particular order so the storage isn't in order.
  tokens.push_back(Token("Cobalt"));
  tokens.push_back(Token("Chrome"));
  tokens.push_back(Token("Firefox"));
  tokens.push_back(Token("Mozilla"));
  tokens.push_back(Token("Navigator"));
  tokens.push_back(Token("Safari"));
  tokens.push_back(Token("Explorer"));
  tokens.push_back(Token("Opera"));

  // Sort the vector so we iterate it alphabetically.
  std::sort(tokens.begin(), tokens.end(),
            [](const Token& l, const Token& r) -> bool {
              return strcmp(l.c_str(), r.c_str()) < 0;
            });

  // The natural order of the tokens is not alphabetical.
  Token prev_token;
  bool first = true;
  bool in_order = true;
  for (const auto token : tokens) {
    if (!first) {
      in_order &= (prev_token < token);
    }
    prev_token = token;
    first = false;
  }
  EXPECT_FALSE(in_order);

  // The order of the tokens should be alphabetical using a sort scope.
  Token::ScopedAlphabeticalSorting sort_scope;
  first = true;
  for (const auto token : tokens) {
    if (!first) {
      EXPECT_LT(prev_token, token);
    }
    prev_token = token;
    first = false;
  }
}

TEST(TokenTest, Collision) {
  const char kTestString[] = "abcdefghijklmnopqrstuvwxyz";
  const uint32 kExtraTokens = 100;
  const uint32 kTotalTokens =
      Token::kHashSlotCount * Token::kStringsPerSlot + kExtraTokens;
  Token empty;

  std::string s = kTestString;
  std::vector<Token> tokens_1;
  for (uint32 i = 0; i < kTotalTokens; ++i) {
    tokens_1.push_back(Token(s));
    // 26! is greater than 10^26, which is large enough for the loop.
    std::next_permutation(s.begin(), s.end());
  }

  s = kTestString;
  std::vector<Token> tokens_2;
  for (uint32 i = 0; i < kTotalTokens; ++i) {
    tokens_2.push_back(Token(s));
    // 26! is greater than 10^26, which is large enough for the loop.
    std::next_permutation(s.begin(), s.end());
  }

  s = kTestString;
  EXPECT_EQ(empty, std::string());
  for (uint32 i = 0; i < kTotalTokens; ++i) {
    EXPECT_EQ(tokens_1[i], s);
    EXPECT_EQ(tokens_1[i], tokens_2[i]);
    std::next_permutation(s.begin(), s.end());
  }
}

}  // namespace
}  // namespace base
