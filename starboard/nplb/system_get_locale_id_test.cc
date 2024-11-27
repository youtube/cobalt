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

#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(SbSystemGetLocaleIdTest, SunnyDay) {
  const char* locale = SbSystemGetLocaleId();
  const char* kNull = NULL;
  EXPECT_NE(kNull, locale);
  EXPECT_NE("C", locale);
  EXPECT_NE("POSIX", locale);

  // Make sure it is consistent on a second call.
  const char* locale2 = SbSystemGetLocaleId();
  EXPECT_EQ(locale, locale2);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
