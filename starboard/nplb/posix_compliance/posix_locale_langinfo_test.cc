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

#include <langinfo.h>
#include <locale.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace posix_compliance {
namespace {

TEST(PosixLocaleTest, NlLanginfo) {
  EXPECT_STREQ(nl_langinfo(RADIXCHAR), ".");
  EXPECT_STREQ(nl_langinfo(THOUSEP), "");
  EXPECT_STREQ(nl_langinfo(CODESET), "US-ASCII");
}

TEST(PosixLocaleTest, NlLanginfoL) {
  locale_t c_locale = newlocale(LC_ALL_MASK, "C", (locale_t)0);
  ASSERT_NE(c_locale, (locale_t)0);
  EXPECT_STREQ(nl_langinfo_l(RADIXCHAR, c_locale), ".");
  EXPECT_STREQ(nl_langinfo_l(THOUSEP, c_locale), "");
  EXPECT_STREQ(nl_langinfo_l(CODESET, c_locale), "US-ASCII");
  freelocale(c_locale);
}

TEST(PosixLocaleTest, NlLanginfoLGlobalLocale) {
  EXPECT_STREQ(nl_langinfo_l(RADIXCHAR, LC_GLOBAL_LOCALE), ".");
  EXPECT_STREQ(nl_langinfo_l(THOUSEP, LC_GLOBAL_LOCALE), "");
  EXPECT_STREQ(nl_langinfo_l(CODESET, LC_GLOBAL_LOCALE), "US-ASCII");
}

TEST(PosixLocaleTest, NlLanginfoLNullLocale) {
  EXPECT_STREQ(nl_langinfo_l(RADIXCHAR, (locale_t)0), "");
  EXPECT_STREQ(nl_langinfo_l(THOUSEP, (locale_t)0), "");
  EXPECT_STREQ(nl_langinfo_l(CODESET, (locale_t)0), "");
}

}  // namespace
}  // namespace posix_compliance
}  // namespace nplb
}  // namespace starboard
