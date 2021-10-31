// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/ui_navigation.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

#if SB_API_VERSION >= 12

// This verifies that the UI navigation API is not implemented.
TEST(UiNavigationTest, GetInterface) {
  SbUiNavInterface interface;
  EXPECT_FALSE(SbUiNavGetInterface(&interface));
}

#endif  // SB_API_VERSION >= 12

}  // namespace.
}  // namespace nplb.
}  // namespace starboard.
