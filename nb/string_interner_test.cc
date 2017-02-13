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

#include <cstring>
#include <string>

#include "nb/string_interner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nb {
namespace {

typedef ::testing::Types<StringInterner, ConcurrentStringInterner>
    StringInternerTypes;

// Defines test type that will be instantiated using each type in
// AllAtomicTypes type list.
template <typename T>
class StringInternerTest : public ::testing::Test {};
TYPED_TEST_CASE(StringInternerTest, StringInternerTypes);  // Registration.

std::string GenerateRandomString(size_t size) {
  static const char valid_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                    "abcdefghijklmnopqrstuvwxyz";
  static const size_t n = strlen(valid_chars);

  std::stringstream ss;
  for (size_t i = 0; i < size; ++i) {
    size_t random_char_index = std::rand() % n;
    ss << valid_chars[random_char_index];
  }
  return ss.str();
}

TYPED_TEST(StringInternerTest, Intern) {
  typedef TypeParam StringInternerT;
  StringInternerT string_interner;
  EXPECT_FALSE(string_interner.Get("A"));
  EXPECT_EQ(&string_interner.Intern("A"), &string_interner.Intern("A"));
  EXPECT_TRUE(string_interner.Get("A"));

  EXPECT_NE(&string_interner.Intern("A"), &string_interner.Intern("B"));
  EXPECT_EQ(2, string_interner.Size());
}

TYPED_TEST(StringInternerTest, InternStdString) {
  typedef TypeParam StringInternerT;
  StringInternerT string_interner;
  std::string a = "A";
  std::string a_copy = "A";
  std::string b = "B";

  EXPECT_FALSE(string_interner.Get(a));
  EXPECT_EQ(&string_interner.Intern(a), &string_interner.Intern(a_copy));
  EXPECT_TRUE(string_interner.Get(a));

  EXPECT_NE(&string_interner.Intern(a), &string_interner.Intern(b));
  EXPECT_EQ(2, string_interner.Size());
}

TYPED_TEST(StringInternerTest, InternBothStrings) {
  typedef TypeParam StringInternerT;
  StringInternerT string_interner;

  std::string a_stdstring = "A";
  const char* c_string = "A";

  const std::string* a_pointer = &string_interner.Intern("A");
  EXPECT_EQ(a_pointer, &string_interner.Intern(c_string));
  EXPECT_EQ(a_pointer, &string_interner.Intern(a_stdstring));
}

// Tests the expectation that a set of unique strings can all be inserted
// into the (Concurrent)StringInterner.
TYPED_TEST(StringInternerTest, LoadLotsOfUniqueStrings) {
  typedef TypeParam StringInternerT;
  StringInternerT string_interner;

  static const size_t kNumStringsToLoad = 1000;

  std::set<std::string> all_unique_strings;

  // Keep generating random strings until we have a full set of unique strings.
  while (all_unique_strings.size() < kNumStringsToLoad) {
    all_unique_strings.insert(GenerateRandomString(25));
  }

  for (std::set<std::string>::const_iterator it = all_unique_strings.begin();
       it != all_unique_strings.end(); ++it) {
    const std::string& str = *it;
    ASSERT_FALSE(string_interner.Get(str));
    ASSERT_FALSE(string_interner.Get(str.c_str()));  // c-string version.

    string_interner.Intern(str);

    ASSERT_TRUE(string_interner.Get(str));
    ASSERT_TRUE(string_interner.Get(str.c_str()));  // c-string version.
  }

  ASSERT_EQ(string_interner.Size(), all_unique_strings.size());
}

}  // namespace
}  // namespace nb
