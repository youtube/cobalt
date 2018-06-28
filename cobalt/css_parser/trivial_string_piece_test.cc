// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/css_parser/trivial_string_piece.h"

#include "gtest/gtest.h"

namespace cobalt {
namespace css_parser {

TEST(TrivialStringPieceTest, CreateFromEmptyCString) {
  TrivialStringPiece string_piece = TrivialStringPiece::FromCString("");
  EXPECT_EQ(0, string_piece.size());
  EXPECT_EQ('\0', *string_piece.begin);
  EXPECT_EQ('\0', *string_piece.end);
  EXPECT_EQ("", string_piece.ToString());
}

TEST(TrivialStringPieceTest, CreateFromNonEmptyCString) {
  TrivialStringPiece string_piece = TrivialStringPiece::FromCString("initial");
  EXPECT_EQ(7, string_piece.size());
  EXPECT_EQ('i', *string_piece.begin);
  EXPECT_EQ('\0', *string_piece.end);
  EXPECT_EQ("initial", string_piece.ToString());
}

TEST(TrivialStringPieceTest, CompareWithEqualString) {
  EXPECT_EQ("inherit", TrivialStringPiece::FromCString("inherit"));
}

TEST(TrivialStringPieceTest, CompareWithShorterString) {
  EXPECT_NE("", TrivialStringPiece::FromCString("inherit"));
}

TEST(TrivialStringPieceTest, CompareWithLongerString) {
  EXPECT_NE("inherit", TrivialStringPiece::FromCString(""));
}

}  // namespace css_parser
}  // namespace cobalt
