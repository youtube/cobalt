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
  EXPECT_EQ(KeywordValue::kAbsolute, KeywordValue::GetAbsolute()->value());
  EXPECT_EQ(KeywordValue::kAuto, KeywordValue::GetAuto()->value());
  EXPECT_EQ(KeywordValue::kBaseline, KeywordValue::GetBaseline()->value());
  EXPECT_EQ(KeywordValue::kBlock, KeywordValue::GetBlock()->value());
  EXPECT_EQ(KeywordValue::kBottom, KeywordValue::GetBottom()->value());
  EXPECT_EQ(KeywordValue::kBreakWord, KeywordValue::GetBreakWord()->value());
  EXPECT_EQ(KeywordValue::kCenter, KeywordValue::GetCenter()->value());
  EXPECT_EQ(KeywordValue::kContain, KeywordValue::GetContain()->value());
  EXPECT_EQ(KeywordValue::kCover, KeywordValue::GetCover()->value());
  EXPECT_EQ(KeywordValue::kCursive, KeywordValue::GetCursive()->value());
  EXPECT_EQ(KeywordValue::kEllipsis, KeywordValue::GetEllipsis()->value());
  EXPECT_EQ(KeywordValue::kEnd, KeywordValue::GetEnd()->value());
  EXPECT_EQ(KeywordValue::kFantasy, KeywordValue::GetFantasy()->value());
  EXPECT_EQ(KeywordValue::kHidden, KeywordValue::GetHidden()->value());
  EXPECT_EQ(KeywordValue::kInherit, KeywordValue::GetInherit()->value());
  EXPECT_EQ(KeywordValue::kInitial, KeywordValue::GetInitial()->value());
  EXPECT_EQ(KeywordValue::kInline, KeywordValue::GetInline()->value());
  EXPECT_EQ(KeywordValue::kInlineBlock,
            KeywordValue::GetInlineBlock()->value());
  EXPECT_EQ(KeywordValue::kLeft, KeywordValue::GetLeft()->value());
  EXPECT_EQ(KeywordValue::kMiddle, KeywordValue::GetMiddle()->value());
  EXPECT_EQ(KeywordValue::kMonospace, KeywordValue::GetMonospace()->value());
  EXPECT_EQ(KeywordValue::kNone, KeywordValue::GetNone()->value());
  EXPECT_EQ(KeywordValue::kNoRepeat, KeywordValue::GetNoRepeat()->value());
  EXPECT_EQ(KeywordValue::kNormal, KeywordValue::GetNormal()->value());
  EXPECT_EQ(KeywordValue::kNoWrap, KeywordValue::GetNoWrap()->value());
  EXPECT_EQ(KeywordValue::kRelative, KeywordValue::GetRelative()->value());
  EXPECT_EQ(KeywordValue::kRepeat, KeywordValue::GetRepeat()->value());
  EXPECT_EQ(KeywordValue::kRight, KeywordValue::GetRight()->value());
  EXPECT_EQ(KeywordValue::kSansSerif, KeywordValue::GetSansSerif()->value());
  EXPECT_EQ(KeywordValue::kSerif, KeywordValue::GetSerif()->value());
  EXPECT_EQ(KeywordValue::kStart, KeywordValue::GetStart()->value());
  EXPECT_EQ(KeywordValue::kStatic, KeywordValue::GetStatic()->value());
  EXPECT_EQ(KeywordValue::kTop, KeywordValue::GetTop()->value());
  EXPECT_EQ(KeywordValue::kUppercase, KeywordValue::GetUppercase()->value());
  EXPECT_EQ(KeywordValue::kVisible, KeywordValue::GetVisible()->value());
}

}  // namespace cssom
}  // namespace cobalt
