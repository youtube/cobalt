// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/common/system_property.h"
#include "starboard/system.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using starboard::kSystemPropertyMaxLength;
using testing::MatchesRegex;

TEST(SbSystemGetPropertyTest, UserAgentAuxField) {
  char out_value[kSystemPropertyMaxLength];
  bool result = SbSystemGetProperty(kSbSystemPropertyUserAgentAuxField,
                                    out_value, kSystemPropertyMaxLength);
  EXPECT_TRUE(result);
  // Assert that the output value matches 4 numbers separated by periods.
  EXPECT_THAT(out_value, MatchesRegex("\\d+\\.\\d+\\.\\d+\\.\\d+"));
}
