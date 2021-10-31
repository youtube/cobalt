/*
 * Copyright 2017 Google Inc. All Rights Reserved.
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

#include "nb/rewindable_vector.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace nb {
namespace {

TEST(RewindableVector, rewind) {
  RewindableVector<std::string> string_vector;
  string_vector.push_back("ABCDEF");
  string_vector.rewind(1);

  EXPECT_TRUE(string_vector.empty());
  EXPECT_EQ(0, string_vector.size());
  EXPECT_EQ(1, string_vector.InternalData().size());

  string_vector.grow(1);
  EXPECT_EQ(1, string_vector.size());
  EXPECT_FALSE(string_vector.empty());

  // This is only possible if the std::string is not destroyed via
  // rewindAll().
  EXPECT_EQ(std::string("ABCDEF"), string_vector[0]);
}

TEST(RewindableVector, rewindAll) {
  RewindableVector<std::string> string_vector;
  string_vector.push_back("ABCDEF");
  string_vector.rewindAll();

  EXPECT_TRUE(string_vector.empty());
  EXPECT_EQ(0, string_vector.size());
  EXPECT_EQ(1, string_vector.InternalData().size());

  string_vector.grow(1);
  EXPECT_EQ(1, string_vector.size());
  EXPECT_FALSE(string_vector.empty());

  // This is only possible if the std::string is not destroyed via
  // rewindAll().
  EXPECT_EQ(std::string("ABCDEF"), string_vector[0]);
}

TEST(RewindableVector, push_pop_back) {
  RewindableVector<std::string> string_vector;
  string_vector.push_back("ABCDEF");
  string_vector.pop_back();

  EXPECT_TRUE(string_vector.empty());
  EXPECT_EQ(0, string_vector.size());
  EXPECT_EQ(1, string_vector.InternalData().size());

  string_vector.grow(1);

  // This is only possible if value was not destroyed during pop_back().
  EXPECT_EQ(std::string("ABCDEF"), string_vector[0]);
}

}  // namespace
}  // namespace nb
