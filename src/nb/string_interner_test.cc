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

#include <string>

#include "nb/string_interner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nb {
namespace {

TEST(StringInterner, Intern) {
  StringInterner string_interner;
  EXPECT_FALSE(string_interner.Get("A"));
  EXPECT_EQ(&string_interner.Intern("A"), &string_interner.Intern("A"));
  EXPECT_TRUE(string_interner.Get("A"));

  EXPECT_NE(&string_interner.Intern("A"), &string_interner.Intern("B"));
  EXPECT_EQ(2, string_interner.Size());
}

TEST(StringInterner, InternStdString) {
  StringInterner string_interner;
  std::string a = "A";
  std::string a_copy = "A";
  std::string b = "B";

  EXPECT_FALSE(string_interner.Get(a));
  EXPECT_EQ(&string_interner.Intern(a), &string_interner.Intern(a_copy));
  EXPECT_TRUE(string_interner.Get(a));

  EXPECT_NE(&string_interner.Intern(a), &string_interner.Intern(b));
  EXPECT_EQ(2, string_interner.Size());
}

}  // namespace
}  // namespace nb
