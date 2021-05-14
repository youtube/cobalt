// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/dom/navigator.h"
#include "cobalt/dom/testing/stub_environment_settings.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom {

// Tests the Navigator::licenses function for non-empty return.
TEST(NavigatorLicensesTest, NonEmpty) {
  testing::StubEnvironmentSettings environment_settings;
  scoped_refptr<cobalt::dom::Navigator> navigator =
      new cobalt::dom::Navigator(&environment_settings, std::string(), NULL,
                                 std::string(), nullptr, nullptr);

  ASSERT_TRUE(navigator != nullptr);
  EXPECT_FALSE(navigator->licenses().empty());
}

}  // namespace dom
}  // namespace cobalt
