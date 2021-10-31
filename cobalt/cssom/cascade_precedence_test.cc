// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/cssom/cascade_precedence.h"

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace cssom {

TEST(CascadePrecedenceTest, SetImportant) {
  CascadePrecedence cascade_precedence_user_agent(kNormalUserAgent);
  CascadePrecedence cascade_precedence_author(kNormalAuthor);
  CascadePrecedence cascade_precedence_override(kNormalOverride);
  cascade_precedence_user_agent.SetImportant();
  cascade_precedence_author.SetImportant();
  cascade_precedence_override.SetImportant();
  EXPECT_EQ(cascade_precedence_user_agent,
            CascadePrecedence(kImportantUserAgent));
  EXPECT_EQ(cascade_precedence_author, CascadePrecedence(kImportantAuthor));
  EXPECT_EQ(cascade_precedence_override, CascadePrecedence(kImportantOverride));
}

TEST(CascadePrecedenceTest, OriginComparison) {
  EXPECT_LT(kNormalUserAgent, kNormalAuthor);
  EXPECT_LT(kNormalAuthor, kImportantAuthor);
  EXPECT_LT(kImportantAuthor, kImportantUserAgent);
}

TEST(CascadePrecedenceTest, AppearanceComparison) {
  EXPECT_LT(Appearance(0, 1), Appearance(0, 2));
  EXPECT_LT(Appearance(0, 1), Appearance(1, 0));
  EXPECT_LT(Appearance(1, 0), Appearance(1, 1));
}

TEST(CascadePrecedenceTest, CascadePrecedenceComparison) {
  std::vector<CascadePrecedence> cascade_priorities;
  cascade_priorities.push_back(CascadePrecedence(
      kNormalUserAgent, Specificity(0, 0, 1), Appearance(0, 0)));
  cascade_priorities.push_back(CascadePrecedence(
      kNormalUserAgent, Specificity(0, 0, 1), Appearance(0, 1)));
  cascade_priorities.push_back(CascadePrecedence(
      kNormalUserAgent, Specificity(0, 1, 0), Appearance(0, 0)));
  cascade_priorities.push_back(CascadePrecedence(
      kNormalUserAgent, Specificity(0, 1, 0), Appearance(0, 1)));
  cascade_priorities.push_back(CascadePrecedence(
      kImportantAuthor, Specificity(0, 0, 1), Appearance(0, 0)));
  cascade_priorities.push_back(CascadePrecedence(
      kImportantAuthor, Specificity(0, 0, 1), Appearance(0, 1)));

  // Check the previous cascade priorities are of ascending order.
  EXPECT_LT(cascade_priorities[0], cascade_priorities[1]);
  EXPECT_LT(cascade_priorities[1], cascade_priorities[2]);
  EXPECT_LT(cascade_priorities[2], cascade_priorities[3]);
  EXPECT_LT(cascade_priorities[3], cascade_priorities[4]);
  EXPECT_LT(cascade_priorities[4], cascade_priorities[5]);
}

}  // namespace cssom
}  // namespace cobalt
