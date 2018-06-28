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

#include "cobalt/cssom/specificity.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace cssom {

TEST(SpecificityTest, Comparison) {
  EXPECT_EQ(Specificity(0, 0, 0), Specificity(0, 0, 0));
  EXPECT_EQ(Specificity(1, 2, 3), Specificity(1, 2, 3));

  EXPECT_LT(Specificity(0, 0, 0), Specificity(0, 0, 1));
  EXPECT_LT(Specificity(0, 0, 1), Specificity(0, 0, 2));
  EXPECT_LT(Specificity(0, 0, 127), Specificity(0, 1, 0));
  EXPECT_LT(Specificity(0, 1, 0), Specificity(0, 1, 1));
  EXPECT_LT(Specificity(0, 127, 127), Specificity(1, 0, 0));
}

TEST(SpecificityTest, AddFrom) {
  Specificity s1(0, 1, 0);
  Specificity s2(1, 0, 1);
  s1.AddFrom(s2);
  EXPECT_EQ(Specificity(1, 1, 1), s1);
}

TEST(SpecificityTest, AddFromClamping) {
  Specificity s1(0, 0, 126);
  Specificity s2(0, 0, 1);
  s1.AddFrom(s2);
  EXPECT_EQ(Specificity(0, 0, 127), s1);

  s1.AddFrom(s2);
  EXPECT_EQ(Specificity(0, 0, 127), s1);

  s1.AddFrom(s2);
  EXPECT_EQ(Specificity(0, 0, 127), s1);
}

}  // namespace cssom
}  // namespace cobalt
