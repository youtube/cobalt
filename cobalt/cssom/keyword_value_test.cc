// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/cssom/keyword_value.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace cssom {

typedef struct {
  KeywordValue::Value key;
  KeywordValue *value;
  const char *name;
} Keywords;

TEST(KeywordValueTest, InstancesAndValuesAreConsistent) {
  Keywords keywords[] = {
      {KeywordValue::kAbsolute, KeywordValue::GetAbsolute(), "absolute"},
      {KeywordValue::kAlternate, KeywordValue::GetAlternate(), "alternate"},
      {KeywordValue::kAlternateReverse, KeywordValue::GetAlternateReverse(),
       "alternate-reverse"},
      {KeywordValue::kAuto, KeywordValue::GetAuto(), "auto"},
      {KeywordValue::kBackwards, KeywordValue::GetBackwards(), "backwards"},
      {KeywordValue::kBaseline, KeywordValue::GetBaseline(), "baseline"},
      {KeywordValue::kBlock, KeywordValue::GetBlock(), "block"},
      {KeywordValue::kBoth, KeywordValue::GetBoth(), "both"},
      {KeywordValue::kBottom, KeywordValue::GetBottom(), "bottom"},
      {KeywordValue::kBreakWord, KeywordValue::GetBreakWord(), "break-word"},
      {KeywordValue::kCenter, KeywordValue::GetCenter(), "center"},
      {KeywordValue::kClip, KeywordValue::GetClip(), "clip"},
      {KeywordValue::kCollapse, KeywordValue::GetCollapse(), "collapse"},
      {KeywordValue::kColumn, KeywordValue::GetColumn(), "column"},
      {KeywordValue::kColumnReverse, KeywordValue::GetColumnReverse(),
       "column-reverse"},
      {KeywordValue::kContain, KeywordValue::GetContain(), "contain"},
      {KeywordValue::kContent, KeywordValue::GetContent(), "content"},
      {KeywordValue::kCover, KeywordValue::GetCover(), "cover"},
      {KeywordValue::kCurrentColor, KeywordValue::GetCurrentColor(),
       "currentColor"},
      {KeywordValue::kCursive, KeywordValue::GetCursive(), "cursive"},
      {KeywordValue::kEllipsis, KeywordValue::GetEllipsis(), "ellipsis"},
      {KeywordValue::kEnd, KeywordValue::GetEnd(), "end"},
      {KeywordValue::kEquirectangular, KeywordValue::GetEquirectangular(),
       "equirectangular"},
      {KeywordValue::kFantasy, KeywordValue::GetFantasy(), "fantasy"},
      {KeywordValue::kFixed, KeywordValue::GetFixed(), "fixed"},
      {KeywordValue::kFlex, KeywordValue::GetFlex(), "flex"},
      {KeywordValue::kFlexEnd, KeywordValue::GetFlexEnd(), "flex-end"},
      {KeywordValue::kFlexStart, KeywordValue::GetFlexStart(), "flex-start"},
      {KeywordValue::kForwards, KeywordValue::GetForwards(), "forwards"},
      {KeywordValue::kHidden, KeywordValue::GetHidden(), "hidden"},
      {KeywordValue::kInfinite, KeywordValue::GetInfinite(), "infinite"},
      {KeywordValue::kInherit, KeywordValue::GetInherit(), "inherit"},
      {KeywordValue::kInitial, KeywordValue::GetInitial(), "initial"},
      {KeywordValue::kInline, KeywordValue::GetInline(), "inline"},
      {KeywordValue::kInlineBlock, KeywordValue::GetInlineBlock(),
       "inline-block"},
      {KeywordValue::kInlineFlex, KeywordValue::GetInlineFlex(), "inline-flex"},
      {KeywordValue::kLeft, KeywordValue::GetLeft(), "left"},
      {KeywordValue::kLineThrough, KeywordValue::GetLineThrough(),
       "line-through"},
      {KeywordValue::kMiddle, KeywordValue::GetMiddle(), "middle"},
      {KeywordValue::kMonoscopic, KeywordValue::GetMonoscopic(), "monoscopic"},
      {KeywordValue::kMonospace, KeywordValue::GetMonospace(), "monospace"},
      {KeywordValue::kNone, KeywordValue::GetNone(), "none"},
      {KeywordValue::kNoRepeat, KeywordValue::GetNoRepeat(), "no-repeat"},
      {KeywordValue::kNormal, KeywordValue::GetNormal(), "normal"},
      {KeywordValue::kNowrap, KeywordValue::GetNowrap(), "nowrap"},
      {KeywordValue::kPre, KeywordValue::GetPre(), "pre"},
      {KeywordValue::kPreLine, KeywordValue::GetPreLine(), "pre-line"},
      {KeywordValue::kPreWrap, KeywordValue::GetPreWrap(), "pre-wrap"},
      {KeywordValue::kRelative, KeywordValue::GetRelative(), "relative"},
      {KeywordValue::kRepeat, KeywordValue::GetRepeat(), "repeat"},
      {KeywordValue::kReverse, KeywordValue::GetReverse(), "reverse"},
      {KeywordValue::kRight, KeywordValue::GetRight(), "right"},
      {KeywordValue::kRow, KeywordValue::GetRow(), "row"},
      {KeywordValue::kRowReverse, KeywordValue::GetRowReverse(), "row-reverse"},
      {KeywordValue::kSansSerif, KeywordValue::GetSansSerif(), "sans-serif"},
      {KeywordValue::kScroll, KeywordValue::GetScroll(), "scroll"},
      {KeywordValue::kSerif, KeywordValue::GetSerif(), "serif"},
      {KeywordValue::kSolid, KeywordValue::GetSolid(), "solid"},
      {KeywordValue::kSpaceAround, KeywordValue::GetSpaceAround(),
       "space-around"},
      {KeywordValue::kSpaceBetween, KeywordValue::GetSpaceBetween(),
       "space-between"},
      {KeywordValue::kStart, KeywordValue::GetStart(), "start"},
      {KeywordValue::kStatic, KeywordValue::GetStatic(), "static"},
      {KeywordValue::kStereoscopicLeftRight,
       KeywordValue::GetStereoscopicLeftRight(), "stereoscopic-left-right"},
      {KeywordValue::kStereoscopicTopBottom,
       KeywordValue::GetStereoscopicTopBottom(), "stereoscopic-top-bottom"},
      {KeywordValue::kStretch, KeywordValue::GetStretch(), "stretch"},
      {KeywordValue::kTop, KeywordValue::GetTop(), "top"},
      {KeywordValue::kUppercase, KeywordValue::GetUppercase(), "uppercase"},
      {KeywordValue::kVisible, KeywordValue::GetVisible(), "visible"},
      {KeywordValue::kWrap, KeywordValue::GetWrap(), "wrap"},
      {KeywordValue::kWrapReverse, KeywordValue::GetWrapReverse(),
       "wrap-reverse"},
  };

  static_assert(SB_ARRAY_SIZE(keywords) == 1 + KeywordValue::kMaxKeywordValue,
                "keywords[] should have a value for each KeywordValue::Value");

  for (unsigned int i = 0; i < SB_ARRAY_SIZE(keywords); ++i) {
    EXPECT_EQ(keywords[i].key, keywords[i].value->value());
    EXPECT_EQ(keywords[i].name, keywords[i].value->ToString());
  }
}

}  // namespace cssom
}  // namespace cobalt
