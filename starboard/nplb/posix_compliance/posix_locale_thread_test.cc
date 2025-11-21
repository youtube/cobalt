// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

// Many systems have only a few locales defined. When a test requires a
// specific locale that is not available, the test will be skipped.

#include <ctype.h>
#include <locale.h>

#include <cerrno>
#include <cstring>

#include "starboard/nplb/posix_compliance/posix_locale_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

constexpr char kDefaultLocale[] = "C";

// This test checks that localeconv() respects the thread-specific locale set
// by uselocale(), even when it differs from the global locale.
class PosixLocaleThreadTest : public ::testing::Test {
 protected:
  void SetUp() override { uselocale(LC_GLOBAL_LOCALE); }
  void TearDown() override { uselocale(LC_GLOBAL_LOCALE); }
};

// This test checks that localeconv() respects the thread-specific locale set
// by uselocale(), even when it differs from the global locale.
TEST_F(PosixLocaleThreadTest, LocaleConvUsesThreadLocale) {
  // Set the global locale to kDefaultLocale, which uses a period for the
  // decimal point.
  setlocale(LC_ALL, kDefaultLocale);

  const char* test_locale = GetCommaDecimalSeparatorLocale();
  if (!test_locale) {
    GTEST_SKIP() << "No supported locale with comma decimal separator found.";
  }

  // Create a locale which uses a comma for the decimal point.
  locale_t loc = newlocale(LC_NUMERIC_MASK, test_locale, (locale_t)0);
  if (!loc) {
    GTEST_SKIP() << "The \"" << test_locale
                 << "\" locale is not supported: " << strerror(errno);
  }

  // Set the thread-specific locale.
  uselocale(loc);

  // localeconv() should now return the properties of the thread-specific
  // locale.
  lconv* lc = localeconv();
  ASSERT_NE(nullptr, lc);
  EXPECT_STREQ(",", lc->decimal_point);

  // Clean up.
  freelocale(loc);
}

TEST_F(PosixLocaleThreadTest, FreeLocaleRestoresGlobalLocale) {
  // Set the global locale to kDefaultLocale, which uses a period for the
  // decimal point.
  setlocale(LC_ALL, kDefaultLocale);

  const char* test_locale = GetCommaDecimalSeparatorLocale();
  if (!test_locale) {
    GTEST_SKIP() << "No supported locale with comma decimal separator found.";
  }

  // Create a locale which uses a comma for the decimal point.
  locale_t loc = newlocale(LC_NUMERIC_MASK, test_locale, (locale_t)0);
  if (!loc) {
    GTEST_SKIP() << "The \"" << test_locale
                 << "\" locale is not supported: " << strerror(errno);
  }

  // Set the thread-specific locale.
  uselocale(loc);
  // Clean up.
  freelocale(loc);

  // After freeing the thread-specific locale, the global locale should be
  // active.
  uselocale(LC_GLOBAL_LOCALE);
  lconv* lc = localeconv();
  ASSERT_NE(nullptr, lc);
  EXPECT_STREQ(".", lc->decimal_point);
}

TEST_F(PosixLocaleThreadTest, NewAndFreeLocale) {
  locale_t loc = newlocale(LC_ALL, kDefaultLocale, (locale_t)0);
  ASSERT_TRUE(loc) << "newlocale failed: " << strerror(errno);
  freelocale(loc);
}

TEST_F(PosixLocaleThreadTest, NewLocaleFreesOrModifiesBase) {
  // Create a locale with only LC_CTYPE set to kDefaultLocale.
  locale_t loc1 = newlocale(LC_CTYPE_MASK, kDefaultLocale, (locale_t)0);
  EXPECT_NE(nullptr, loc1);

  const char* test_locale = GetCommaDecimalSeparatorLocale();
  if (!test_locale) {
    GTEST_SKIP() << "No supported locale with comma decimal separator found.";
  }

  // Now, modify loc1 by setting LC_NUMERIC to test_locale.
  // The resulting locale loc2 should have both properties.
  locale_t loc2 = newlocale(LC_NUMERIC_MASK, test_locale, loc1);
  if (!loc2) {
    GTEST_SKIP() << "The \"" << test_locale
                 << "\" locale is not supported: " << strerror(errno);
  }

  // To verify, we can query the locale's properties.
  // We can't directly inspect a locale_t object in a standard way,
  // but we can use it and then check the behavior.
  locale_t previous_locale = uselocale(loc2);

  // Check numeric properties from test_locale
  lconv* lc = localeconv();
  ASSERT_NE(nullptr, lc);
  EXPECT_STREQ(",", lc->decimal_point);

  // Check ctype properties from kDefaultLocale
  // For kDefaultLocale and test_locale, isupper('A') should be true.
  EXPECT_NE(0, isupper('A'));

  // Restore previous locale
  uselocale(previous_locale);

  freelocale(loc2);
}

TEST_F(PosixLocaleThreadTest, DupLocale) {
  locale_t loc1 = newlocale(LC_ALL, kDefaultLocale, (locale_t)0);
  ASSERT_NE(nullptr, loc1);
  locale_t loc2 = duplocale(loc1);
  EXPECT_NE(nullptr, loc2);

  const char* test_locale = GetCommaDecimalSeparatorLocale();
  if (!test_locale) {
    GTEST_SKIP() << "No supported locale with comma decimal separator found.";
  }

  locale_t loc3 = newlocale(LC_ALL, test_locale, loc2);
  EXPECT_NE(loc3, loc1);

  locale_t previous_locale = uselocale(loc1);
  lconv* lc1 = localeconv();
  ASSERT_NE(nullptr, lc1);
  EXPECT_STREQ(".", lc1->decimal_point);

  uselocale(loc3);
  lconv* lc3 = localeconv();
  ASSERT_NE(nullptr, lc3);
  EXPECT_STREQ(",", lc3->decimal_point);

  uselocale(previous_locale);

  freelocale(loc1);
  freelocale(loc2);
  freelocale(loc3);
}

TEST_F(PosixLocaleThreadTest, DupLocaleNonDefault) {
  const char* test_locale = GetCommaDecimalSeparatorLocale();
  if (!test_locale) {
    GTEST_SKIP() << "No supported locale with comma decimal separator found.";
  }
  locale_t loc1 = newlocale(LC_ALL, test_locale, (locale_t)0);
  if (!loc1) {
    GTEST_SKIP() << "The \"" << test_locale
                 << "\" locale is not supported: " << strerror(errno);
  }
  locale_t loc2 = duplocale(loc1);
  EXPECT_NE(nullptr, loc2);
  EXPECT_NE(loc1, loc2);

  locale_t previous_locale = uselocale(loc1);
  lconv* lc1 = localeconv();
  ASSERT_TRUE(lc1);
  EXPECT_STREQ(",", lc1->decimal_point);

  uselocale(loc2);
  lconv* lc2 = localeconv();
  ASSERT_TRUE(lc2);
  EXPECT_STREQ(",", lc2->decimal_point);

  EXPECT_STREQ(lc1->decimal_point, lc2->decimal_point);
  uselocale(previous_locale);

  freelocale(loc1);
  freelocale(loc2);
}

TEST_F(PosixLocaleThreadTest, DupLocaleGlobal) {
  locale_t loc = duplocale(LC_GLOBAL_LOCALE);
  EXPECT_NE(nullptr, loc);
  EXPECT_NE(LC_GLOBAL_LOCALE, loc);
}

TEST_F(PosixLocaleThreadTest, UseLocale) {
  locale_t loc = newlocale(LC_ALL, kDefaultLocale, (locale_t)0);
  EXPECT_NE(nullptr, loc);
  locale_t old_loc = uselocale(loc);
  EXPECT_EQ(LC_GLOBAL_LOCALE, old_loc);
  EXPECT_EQ(loc, uselocale((locale_t)0));
  freelocale(loc);
}

// This test checks that when setlocale() is called to set the global locale,
// and then uselocale() is called to set a thread-specific locale, the
// thread-specific locale is the one that is active.
TEST_F(PosixLocaleThreadTest, SetLocaleThenUseLocale) {
  setlocale(LC_ALL, kDefaultLocale);
  const char* test_locale = GetNonDefaultLocale();
  if (!test_locale) {
    GTEST_SKIP() << "No supported non-default locale found.";
  }
  locale_t loc = newlocale(LC_ALL, test_locale, (locale_t)0);
  if (!loc) {
    GTEST_SKIP() << "The \"" << test_locale
                 << "\" locale is not supported: " << strerror(errno);
  }
  uselocale(loc);
  EXPECT_EQ(loc, uselocale((locale_t)0));
  freelocale(loc);
}

// This test checks that when setlocale() is called to set the global locale,
// and then uselocale() is called to set a thread-specific locale, and then
// uselocale() is called with LC_GLOBAL_LOCALE, the global locale is the one
// that is active.
TEST_F(PosixLocaleThreadTest, SetLocaleThenUseLocaleThenUseGlobal) {
  setlocale(LC_ALL, kDefaultLocale);
  const char* test_locale = GetNonDefaultLocale();
  if (!test_locale) {
    GTEST_SKIP() << "No supported non-default locale found.";
  }
  locale_t loc = newlocale(LC_ALL, test_locale, (locale_t)0);
  if (!loc) {
    GTEST_SKIP() << "The \"" << test_locale
                 << "\" locale is not supported: " << strerror(errno);
  }
  uselocale(loc);
  EXPECT_EQ(loc, uselocale((locale_t)0));
  uselocale(LC_GLOBAL_LOCALE);
  EXPECT_EQ(LC_GLOBAL_LOCALE, uselocale((locale_t)0));
  freelocale(loc);
}

// This test checks that when uselocale() is called to set a thread-specific
// locale, and then setlocale() is called to set the global locale, the
// thread-specific locale is the one that is active.
TEST_F(PosixLocaleThreadTest, UseLocaleThenSetLocale) {
  const char* test_locale = GetNonDefaultLocale();
  if (!test_locale) {
    GTEST_SKIP() << "No supported non-default locale found.";
  }
  locale_t loc = newlocale(LC_ALL, test_locale, (locale_t)0);
  if (!loc) {
    GTEST_SKIP() << "The \"" << test_locale
                 << "\" locale is not supported: " << strerror(errno);
  }
  uselocale(loc);
  setlocale(LC_ALL, kDefaultLocale);
  EXPECT_EQ(loc, uselocale((locale_t)0));
  freelocale(loc);
}

// This test checks that when uselocale() is called to set a thread-specific
// locale, and then setlocale() is called to set the global locale, and then
// uselocale() is called with LC_GLOBAL_LOCALE, the global locale is the one
// that is active.
TEST_F(PosixLocaleThreadTest, UseLocaleThenSetLocaleThenUseGlobal) {
  const char* test_locale = GetNonDefaultLocale();
  if (!test_locale) {
    GTEST_SKIP() << "No supported non-default locale found.";
  }
  locale_t loc = newlocale(LC_ALL, test_locale, (locale_t)0);
  if (!loc) {
    GTEST_SKIP() << "The \"" << test_locale
                 << "\" locale is not supported: " << strerror(errno);
  }
  uselocale(loc);
  setlocale(LC_ALL, kDefaultLocale);
  uselocale(LC_GLOBAL_LOCALE);
  EXPECT_EQ(LC_GLOBAL_LOCALE, uselocale((locale_t)0));
  freelocale(loc);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
