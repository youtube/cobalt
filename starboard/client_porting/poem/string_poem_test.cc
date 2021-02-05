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

// Test basic functionality of string poems.

#include "testing/gtest/include/gtest/gtest.h"

#include "starboard/client_porting/poem/string_poem.h"

namespace starboard {
namespace nplb {
namespace {

TEST(StringPoemTest, PoemStringConcat) {
  char a[64] = "Hello ";
  char b[] = "Yu";

  strncat(a, b, sizeof(b) + 1);

  EXPECT_STREQ(a, "Hello Yu");
}

TEST(StringPoemTest, PoemStringCopy) {
  char a[] = "Hello ";
  char b[] = "Yu";

  strncpy(a, b, sizeof(b));

  EXPECT_STREQ(a, "Yu");
}

TEST(StringPoemTest, PoemStringCopyZeroBytes) {
  char a[] = "Hello ";
  char b[] = "Yu";

  strncpy(a, b, 0);

  EXPECT_STREQ(a, "Hello ");
}

TEST(StringPoemTest, PoemStringCopySrcNotNullTerminated) {
  char a[] = "Hello ";
  char b[] = "Yu";

  strncpy(a, b, sizeof(b) - 1);

  EXPECT_STREQ(a, "Yullo ");
}

TEST(StringPoemTest, PoemStringCopyNotEnoughSpace) {
  char a[] = "Hello";
  char b[] = "Cobalt";

  strncpy(a, b, sizeof(a) - 1);

  EXPECT_STREQ(a, "Cobal");
}

TEST(StringPoemTest, PoemStringCopySizeMoreThanNeeded) {
  char a[] = "Hello";
  char b[] = "Wu";

  strncpy(a, b, sizeof(a));

  EXPECT_STREQ(a, "Wu");
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
