// Copyright 2025 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License-for-dev at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <locale.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(PosixLocaleThreadTest, SetLocaleReturnsC) {
  EXPECT_STREQ("C", setlocale(LC_ALL, "C"));
}

TEST(PosixLocaleThreadTest, SetLocaleReturnsNull) {
  EXPECT_EQ(NULL, setlocale(LC_ALL, "en_US.UTF-8"));
}

TEST(PosixLocaleThreadTest, LocaleConvReturnsCLconv) {
  lconv* lc = localeconv();
  EXPECT_STREQ(".", lc->decimal_point);
}

TEST(PosixLocaleThreadTest, NewAndFreeLocale) {
  locale_t loc = newlocale(LC_ALL, "C", (locale_t)0);
  EXPECT_NE(nullptr, loc);
  lconv* lc = reinterpret_cast<lconv*>(loc);
  EXPECT_STREQ(".", lc->decimal_point);
  freelocale(loc);
}

TEST(PosixLocaleThreadTest, NewLocaleFreesOrModifiesBase) {
  locale_t loc1 = newlocale(LC_ALL, "C", (locale_t)0);
  EXPECT_NE(nullptr, loc1);
  locale_t loc2 = newlocale(LC_ALL, "C", loc1);
  EXPECT_NE(nullptr, loc2);
  EXPECT_NE(loc1, loc2);
  // loc1 should have been freed by the second call to newlocale.
  lconv* lc2 = reinterpret_cast<lconv*>(loc2);
  EXPECT_STREQ(".", lc2->decimal_point);
  freelocale(loc2);
}

TEST(PosixLocaleThreadTest, DupLocale) {
  locale_t loc1 = newlocale(LC_ALL, "C", (locale_t)0);
  EXPECT_NE(nullptr, loc1);
  locale_t loc2 = duplocale(loc1);
  EXPECT_NE(nullptr, loc2);
  EXPECT_NE(loc1, loc2);
  lconv* lc1 = reinterpret_cast<lconv*>(loc1);
  lconv* lc2 = reinterpret_cast<lconv*>(loc2);
  EXPECT_STREQ(lc1->decimal_point, lc2->decimal_point);
  freelocale(loc1);
  freelocale(loc2);
}

TEST(PosixLocaleThreadTest, DupLocaleGlobal) {
  locale_t loc = duplocale(LC_GLOBAL_LOCALE);
  EXPECT_EQ(LC_GLOBAL_LOCALE, loc);
}

TEST(PosixLocaleThreadTest, UseLocale) {
  locale_t loc = newlocale(LC_ALL, "C", (locale_t)0);
  EXPECT_NE(nullptr, loc);
  locale_t old_loc = uselocale(loc);
  EXPECT_EQ(LC_GLOBAL_LOCALE, old_loc);
  EXPECT_EQ(loc, uselocale((locale_t)0));
  freelocale(loc);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
