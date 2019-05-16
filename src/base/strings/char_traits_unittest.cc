// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/char_traits.h"
#include "base/strings/string16.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "base/cpp14oncpp11.h"

namespace base {

TEST(CharTraitsTest, CharCompare) {
  STATIC_ASSERT(CharTraits<char>::compare("abc", "def", 3) == -1, "");
  STATIC_ASSERT(CharTraits<char>::compare("def", "def", 3) == 0, "");
  STATIC_ASSERT(CharTraits<char>::compare("ghi", "def", 3) == 1, "");
}

TEST(CharTraitsTest, CharLength) {
  STATIC_ASSERT(CharTraits<char>::length("") == 0, "");
  STATIC_ASSERT(CharTraits<char>::length("abc") == 3, "");
}

TEST(CharTraitsTest, Char16TCompare) {
  STATIC_ASSERT(CharTraits<char16_t>::compare(u"abc", u"def", 3) == -1, "");
  STATIC_ASSERT(CharTraits<char16_t>::compare(u"def", u"def", 3) == 0, "");
  STATIC_ASSERT(CharTraits<char16_t>::compare(u"ghi", u"def", 3) == 1, "");
}

TEST(CharTraitsTest, Char16TLength) {
  STATIC_ASSERT(CharTraits<char16_t>::length(u"abc") == 3, "");
}

}  // namespace base
