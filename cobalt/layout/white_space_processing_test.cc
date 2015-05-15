/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/layout/white_space_processing.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace layout {

TEST(CollapseWhiteSpaceTest, Empty) {
  std::string text = "";
  CollapseWhiteSpace(&text);
  EXPECT_EQ("", text);
}

TEST(CollapseWhiteSpaceTest, SingleSpace) {
  std::string text = " ";
  CollapseWhiteSpace(&text);
  EXPECT_EQ(" ", text);
}

TEST(CollapseWhiteSpaceTest, DoubleSpace) {
  std::string text = "  ";
  CollapseWhiteSpace(&text);
  EXPECT_EQ(" ", text);
}

TEST(CollapseWhiteSpaceTest, Word) {
  std::string text = "hello";
  CollapseWhiteSpace(&text);
  EXPECT_EQ("hello", text);
}

TEST(CollapseWhiteSpaceTest, WordSurroundedBySingleSpaces) {
  std::string text = " hello ";
  CollapseWhiteSpace(&text);
  EXPECT_EQ(" hello ", text);
}

TEST(CollapseWhiteSpaceTest, WordSurroundedByDoubleSpaces) {
  std::string text = "  hello  ";
  CollapseWhiteSpace(&text);
  EXPECT_EQ(" hello ", text);
}

TEST(CollapseWhiteSpaceTest, TwoWordsSeparatedBySingleSpace) {
  std::string text = "Hello, world!";
  CollapseWhiteSpace(&text);
  EXPECT_EQ("Hello, world!", text);
}

TEST(CollapseWhiteSpaceTest, TwoWordsSurroundedAndSeparatedByDoubleSpaces) {
  std::string text = "  Hello,  world!  ";
  CollapseWhiteSpace(&text);
  EXPECT_EQ(" Hello, world! ", text);
}

TEST(TrimWhiteSpaceTest, Empty) {
  std::string text = "";
  bool has_leading_white_space;
  bool has_trailing_white_space;

  TrimWhiteSpace(&text, &has_leading_white_space, &has_trailing_white_space);

  EXPECT_EQ("", text);
  EXPECT_FALSE(has_leading_white_space);
  EXPECT_FALSE(has_trailing_white_space);
}

TEST(TrimWhiteSpaceTest, Space) {
  std::string text = " ";
  bool has_leading_white_space;
  bool has_trailing_white_space;

  TrimWhiteSpace(&text, &has_leading_white_space, &has_trailing_white_space);

  EXPECT_EQ("", text);
  EXPECT_TRUE(has_leading_white_space);
  EXPECT_TRUE(has_trailing_white_space);
}

TEST(TrimWhiteSpaceTest, Word) {
  std::string text = "hello";
  bool has_leading_white_space;
  bool has_trailing_white_space;

  TrimWhiteSpace(&text, &has_leading_white_space, &has_trailing_white_space);

  EXPECT_EQ("hello", text);
  EXPECT_FALSE(has_leading_white_space);
  EXPECT_FALSE(has_trailing_white_space);
}

TEST(TrimWhiteSpaceTest, WordSurroundedBySpaces) {
  std::string text = " hello ";
  bool has_leading_white_space;
  bool has_trailing_white_space;

  TrimWhiteSpace(&text, &has_leading_white_space, &has_trailing_white_space);

  EXPECT_EQ("hello", text);
  EXPECT_TRUE(has_leading_white_space);
  EXPECT_TRUE(has_trailing_white_space);
}

}  // namespace layout
}  // namespace cobalt
