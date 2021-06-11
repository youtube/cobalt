// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace android {
namespace shared {
namespace {

TEST(ModelYearTest, YearIsFourDigitsOrUnknown) {
  const size_t kValueSize = 1024;
  char value[kValueSize] = {0};
  memset(value, 0xCD, kValueSize);
  bool result =
      SbSystemGetProperty(kSbSystemPropertyModelYear, value, kValueSize);
  SB_DCHECK(result);
  std::string year = value;
  if (year == "unknown") {
    return;
  }
  EXPECT_EQ(4, year.length());
  EXPECT_EQ(std::string::npos, year.find_first_not_of("0123456789"));
  EXPECT_EQ("20", year.substr(0, 2));
}

}  // namespace
}  // namespace shared
}  // namespace android
}  // namespace starboard
