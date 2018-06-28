// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

TEST(FindNextNewlineSequenceTest, NoNewlineSequences) {
  std::string text = "Hello, world!";
  size_t start_index = 0;
  size_t sequence_index;
  size_t sequence_length;

  bool foundSequence = FindNextNewlineSequence(
      text, start_index, &sequence_index, &sequence_length);
  EXPECT_FALSE(foundSequence);
  EXPECT_EQ(sequence_index, text.size());
  EXPECT_EQ(sequence_length, 0);
}

TEST(FindNextNewlineSequenceTest, CarriageReturnSequence) {
  std::string text = "Hello,\r world!";
  size_t start_index = 0;
  size_t sequence_index;
  size_t sequence_length;

  bool foundSequence = FindNextNewlineSequence(
      text, start_index, &sequence_index, &sequence_length);
  EXPECT_TRUE(foundSequence);
  EXPECT_EQ(sequence_index, 6);
  EXPECT_EQ(sequence_length, 1);

  start_index = sequence_index + sequence_length;
  foundSequence = FindNextNewlineSequence(text, start_index, &sequence_index,
                                          &sequence_length);
  EXPECT_FALSE(foundSequence);
  EXPECT_EQ(sequence_index, text.size());
  EXPECT_EQ(sequence_length, 0);
}

TEST(FindNextNewlineSequenceTest, CRLFSequence) {
  std::string text = "Hello,\r\n world!";
  size_t start_index = 0;
  size_t sequence_index;
  size_t sequence_length;

  bool foundSequence = FindNextNewlineSequence(
      text, start_index, &sequence_index, &sequence_length);
  EXPECT_TRUE(foundSequence);
  EXPECT_EQ(sequence_index, 6);
  EXPECT_EQ(sequence_length, 2);

  start_index = sequence_index + sequence_length;
  foundSequence = FindNextNewlineSequence(text, start_index, &sequence_index,
                                          &sequence_length);
  EXPECT_FALSE(foundSequence);
  EXPECT_EQ(sequence_index, text.size());
  EXPECT_EQ(sequence_length, 0);
}

TEST(FindNextNewlineSequenceTest, LineFeedSequence) {
  std::string text = "Hello,\n world!";
  size_t start_index = 0;
  size_t sequence_index;
  size_t sequence_length;

  bool foundSequence = FindNextNewlineSequence(
      text, start_index, &sequence_index, &sequence_length);
  EXPECT_TRUE(foundSequence);
  EXPECT_EQ(sequence_index, 6);
  EXPECT_EQ(sequence_length, 1);

  start_index = sequence_index + sequence_length;
  foundSequence = FindNextNewlineSequence(text, start_index, &sequence_index,
                                          &sequence_length);
  EXPECT_FALSE(foundSequence);
  EXPECT_EQ(sequence_index, text.size());
  EXPECT_EQ(sequence_length, 0);
}

TEST(FindNextNewlineSequenceTest, NewLineSequenceAtStart) {
  std::string text = "\nHello, world!";
  size_t start_index = 0;
  size_t sequence_index;
  size_t sequence_length;

  bool foundSequence = FindNextNewlineSequence(
      text, start_index, &sequence_index, &sequence_length);
  EXPECT_TRUE(foundSequence);
  EXPECT_EQ(sequence_index, 0);
  EXPECT_EQ(sequence_length, 1);

  start_index = sequence_index + sequence_length;
  foundSequence = FindNextNewlineSequence(text, start_index, &sequence_index,
                                          &sequence_length);
  EXPECT_FALSE(foundSequence);
  EXPECT_EQ(sequence_index, text.size());
  EXPECT_EQ(sequence_length, 0);
}

TEST(FindNextNewlineSequenceTest, ConsecutiveNewLineSequences) {
  std::string text = "Hello,\n\n world!";
  size_t start_index = 0;
  size_t sequence_index;
  size_t sequence_length;

  bool foundSequence = FindNextNewlineSequence(
      text, start_index, &sequence_index, &sequence_length);
  EXPECT_TRUE(foundSequence);
  EXPECT_EQ(sequence_index, 6);
  EXPECT_EQ(sequence_length, 1);

  start_index = sequence_index + sequence_length;
  foundSequence = FindNextNewlineSequence(text, start_index, &sequence_index,
                                          &sequence_length);
  EXPECT_TRUE(foundSequence);
  EXPECT_EQ(sequence_index, 7);
  EXPECT_EQ(sequence_length, 1);

  start_index = sequence_index + sequence_length;
  foundSequence = FindNextNewlineSequence(text, start_index, &sequence_index,
                                          &sequence_length);
  EXPECT_FALSE(foundSequence);
  EXPECT_EQ(sequence_index, text.size());
  EXPECT_EQ(sequence_length, 0);
}

TEST(FindNextNewlineSequenceTest, NewLineAtEnd) {
  std::string text = "Hello, world!\n";
  size_t start_index = 0;
  size_t sequence_index;
  size_t sequence_length;

  bool foundSequence = FindNextNewlineSequence(
      text, start_index, &sequence_index, &sequence_length);
  EXPECT_TRUE(foundSequence);
  EXPECT_EQ(sequence_index, 13);
  EXPECT_EQ(sequence_length, 1);

  start_index = sequence_index + sequence_length;
  foundSequence = FindNextNewlineSequence(text, start_index, &sequence_index,
                                          &sequence_length);
  EXPECT_FALSE(foundSequence);
  EXPECT_EQ(sequence_index, text.size());
  EXPECT_EQ(sequence_length, 0);
}

TEST(FindNextNewlineSequenceTest, MultipleNewLineSequences) {
  std::string text = "\nHello,\r\n \nworld!\r";
  size_t start_index = 0;
  size_t sequence_index;
  size_t sequence_length;

  bool foundSequence = FindNextNewlineSequence(
      text, start_index, &sequence_index, &sequence_length);
  EXPECT_TRUE(foundSequence);
  EXPECT_EQ(sequence_index, 0);
  EXPECT_EQ(sequence_length, 1);

  start_index = sequence_index + sequence_length;
  foundSequence = FindNextNewlineSequence(text, start_index, &sequence_index,
                                          &sequence_length);
  EXPECT_TRUE(foundSequence);
  EXPECT_EQ(sequence_index, 7);
  EXPECT_EQ(sequence_length, 2);

  start_index = sequence_index + sequence_length;
  foundSequence = FindNextNewlineSequence(text, start_index, &sequence_index,
                                          &sequence_length);
  EXPECT_TRUE(foundSequence);
  EXPECT_EQ(sequence_index, 10);
  EXPECT_EQ(sequence_length, 1);

  start_index = sequence_index + sequence_length;
  foundSequence = FindNextNewlineSequence(text, start_index, &sequence_index,
                                          &sequence_length);
  EXPECT_TRUE(foundSequence);
  EXPECT_EQ(sequence_index, 17);
  EXPECT_EQ(sequence_length, 1);

  start_index = sequence_index + sequence_length;
  foundSequence = FindNextNewlineSequence(text, start_index, &sequence_index,
                                          &sequence_length);
  EXPECT_FALSE(foundSequence);
  EXPECT_EQ(sequence_index, text.size());
  EXPECT_EQ(sequence_length, 0);
}

}  // namespace layout
}  // namespace cobalt
