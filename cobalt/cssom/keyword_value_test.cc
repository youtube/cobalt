/*
 * Copyright 2014 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/cssom/keyword_value.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace cssom {

TEST(KeywordValueTest, InstancesAndValuesAreConsistent) {
  EXPECT_EQ(KeywordValue::kAuto, KeywordValue::GetAuto()->value());
  EXPECT_EQ(KeywordValue::kBlock, KeywordValue::GetBlock()->value());
  EXPECT_EQ(KeywordValue::kHidden, KeywordValue::GetHidden()->value());
  EXPECT_EQ(KeywordValue::kInherit, KeywordValue::GetInherit()->value());
  EXPECT_EQ(KeywordValue::kInitial, KeywordValue::GetInitial()->value());
  EXPECT_EQ(KeywordValue::kInline, KeywordValue::GetInline()->value());
  EXPECT_EQ(KeywordValue::kInlineBlock,
            KeywordValue::GetInlineBlock()->value());
  EXPECT_EQ(KeywordValue::kNone, KeywordValue::GetNone()->value());
  EXPECT_EQ(KeywordValue::kNormal, KeywordValue::GetNormal()->value());
  EXPECT_EQ(KeywordValue::kVisible, KeywordValue::GetVisible()->value());
}

}  // namespace cssom
}  // namespace cobalt
