// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/common/libc/locale/locale.h"

#include <locale.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(LocaleTest, SetLocaleReturnsC) {
  EXPECT_STREQ("C", setlocale(LC_ALL, "C"));
}

TEST(LocaleTest, SetLocaleReturnsNull) {
  EXPECT_EQ(NULL, setlocale(LC_ALL, "en_US.UTF-8"));
}

TEST(LocaleTest, LocaleConvReturnsCLconv) {
  lconv* lc = localeconv();
  EXPECT_STREQ(".", lc->decimal_point);
}

}  // namespace
