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

#include "starboard/extension/ui_navigation.h"

#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

// This verifies that the UI navigation API is not implemented.
TEST(UiNavigationTest, GetInterface) {
  const SbUiNavInterface* interface = static_cast<const SbUiNavInterface*>(
      SbSystemGetExtension(kCobaltExtensionUiNavigationName));
  EXPECT_FALSE(interface != nullptr);
}

}  // namespace.
}  // namespace nplb.
