// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/cssom/css_declared_style_data.h"

#include "cobalt/cssom/css_style_sheet.h"
#include "cobalt/cssom/font_weight_value.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/property_definitions.h"
#include "cobalt/cssom/rgba_color_value.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace cssom {

TEST(CSSDeclaredStyleDataTest, BackgroundColorSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kBackgroundColorProperty));

  style->SetPropertyValueAndImportance(kBackgroundColorProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kBackgroundColorProperty));
}

TEST(CSSDeclaredStyleDataTest, BackgroundImageSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kBackgroundImageProperty));

  style->SetPropertyValueAndImportance(kBackgroundImageProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kBackgroundImageProperty));
}

TEST(CSSDeclaredStyleDataTest,
     BackgroundPositionSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kBackgroundPositionProperty));

  style->SetPropertyValueAndImportance(kBackgroundPositionProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kBackgroundPositionProperty));
}

TEST(CSSDeclaredStyleDataTest, BackgroundRepeatSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kBackgroundRepeatProperty));

  style->SetPropertyValueAndImportance(kBackgroundRepeatProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kBackgroundRepeatProperty));
}

TEST(CSSDeclaredStyleDataTest, BackgroundSizeSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kBackgroundSizeProperty));

  style->SetPropertyValueAndImportance(kBackgroundSizeProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kBackgroundSizeProperty));
}

TEST(CSSDeclaredStyleDataTest, BorderRadiusSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kBorderTopLeftRadiusProperty));

  style->SetPropertyValueAndImportance(kBorderTopLeftRadiusProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kBorderTopLeftRadiusProperty));

  EXPECT_FALSE(style->GetPropertyValue(kBorderTopRightRadiusProperty));

  style->SetPropertyValueAndImportance(kBorderTopRightRadiusProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kBorderTopRightRadiusProperty));

  EXPECT_FALSE(style->GetPropertyValue(kBorderBottomRightRadiusProperty));

  style->SetPropertyValueAndImportance(kBorderBottomRightRadiusProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kBorderBottomRightRadiusProperty));

  EXPECT_FALSE(style->GetPropertyValue(kBorderBottomLeftRadiusProperty));

  style->SetPropertyValueAndImportance(kBorderBottomLeftRadiusProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kBorderBottomLeftRadiusProperty));
}

TEST(CSSDeclaredStyleDataTest, BorderTopColorSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kBorderTopColorProperty));

  style->SetPropertyValueAndImportance(kBorderTopColorProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kBorderTopColorProperty));
}

TEST(CSSDeclaredStyleDataTest, BorderRightColorSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kBorderRightColorProperty));

  style->SetPropertyValueAndImportance(kBorderRightColorProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kBorderRightColorProperty));
}

TEST(CSSDeclaredStyleDataTest,
     BorderBottomColorSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kBorderBottomColorProperty));

  style->SetPropertyValueAndImportance(kBorderBottomColorProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kBorderBottomColorProperty));
}

TEST(CSSDeclaredStyleDataTest, BorderLeftColorSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kBorderLeftColorProperty));

  style->SetPropertyValueAndImportance(kBorderLeftColorProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kBorderLeftColorProperty));
}

TEST(CSSDeclaredStyleDataTest, BorderTopStyleSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kBorderTopStyleProperty));

  style->SetPropertyValueAndImportance(kBorderTopStyleProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kBorderTopStyleProperty));
}

TEST(CSSDeclaredStyleDataTest, BorderRightStyleSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kBorderRightStyleProperty));

  style->SetPropertyValueAndImportance(kBorderRightStyleProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kBorderRightStyleProperty));
}

TEST(CSSDeclaredStyleDataTest,
     BorderBottomStyleSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kBorderBottomStyleProperty));

  style->SetPropertyValueAndImportance(kBorderBottomStyleProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kBorderBottomStyleProperty));
}

TEST(CSSDeclaredStyleDataTest, BorderLeftStyleSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kBorderLeftStyleProperty));

  style->SetPropertyValueAndImportance(kBorderLeftStyleProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kBorderLeftStyleProperty));
}

TEST(CSSDeclaredStyleDataTest, BorderTopWidthSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kBorderTopWidthProperty));

  style->SetPropertyValueAndImportance(kBorderTopWidthProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kBorderTopWidthProperty));
}

TEST(CSSDeclaredStyleDataTest, BorderRightWidthSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kBorderRightWidthProperty));

  style->SetPropertyValueAndImportance(kBorderRightWidthProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kBorderRightWidthProperty));
}

TEST(CSSDeclaredStyleDataTest,
     BorderBottomWidthSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kBorderBottomWidthProperty));

  style->SetPropertyValueAndImportance(kBorderBottomWidthProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kBorderBottomWidthProperty));
}

TEST(CSSDeclaredStyleDataTest, BorderLeftWidthSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kBorderLeftWidthProperty));

  style->SetPropertyValueAndImportance(kBorderLeftWidthProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kBorderLeftWidthProperty));
}

TEST(CSSDeclaredStyleDataTest, ColorSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kColorProperty));

  style->SetPropertyValueAndImportance(kColorProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kColorProperty));
}

TEST(CSSDeclaredStyleDataTest, ContentSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kContentProperty));

  style->SetPropertyValueAndImportance(kContentProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kContentProperty));
}

TEST(CSSDeclaredStyleDataTest, DisplaySettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kDisplayProperty));

  style->SetPropertyValueAndImportance(kDisplayProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kDisplayProperty));
}

TEST(CSSDeclaredStyleDataTest, FilterSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kFilterProperty));

  style->SetPropertyValueAndImportance(kFilterProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kFilterProperty));
}

TEST(CSSDeclaredStyleDataTest, FontFamilySettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kFontFamilyProperty));

  style->SetPropertyValueAndImportance(kFontFamilyProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kFontFamilyProperty));
}

TEST(CSSDeclaredStyleDataTest, FontSizeSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kFontSizeProperty));

  style->SetPropertyValueAndImportance(kFontSizeProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kFontSizeProperty));
}

TEST(CSSDeclaredStyleDataTest, FontStyleSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kFontStyleProperty));

  style->SetPropertyValueAndImportance(kFontStyleProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kFontStyleProperty));
}

TEST(CSSDeclaredStyleDataTest, FontWeightSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kFontWeightProperty));

  style->SetPropertyValueAndImportance(kFontWeightProperty,
                                       FontWeightValue::GetBoldAka700(), false);
  EXPECT_EQ(FontWeightValue::GetBoldAka700(),
            style->GetPropertyValue(kFontWeightProperty));
}

TEST(CSSDeclaredStyleDataTest, HeightSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kHeightProperty));

  style->SetPropertyValueAndImportance(kHeightProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kHeightProperty));
}

TEST(CSSDeclaredStyleDataTest, LineHeightSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kLineHeightProperty));

  style->SetPropertyValueAndImportance(kLineHeightProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kLineHeightProperty));
}

TEST(CSSDeclaredStyleDataTest, MarginBottomSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kMarginBottomProperty));

  style->SetPropertyValueAndImportance(kMarginBottomProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kMarginBottomProperty));
}

TEST(CSSDeclaredStyleDataTest, MarginLeftSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kMarginLeftProperty));

  style->SetPropertyValueAndImportance(kMarginLeftProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kMarginLeftProperty));
}

TEST(CSSDeclaredStyleDataTest, MarginRightSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kMarginRightProperty));

  style->SetPropertyValueAndImportance(kMarginRightProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kMarginRightProperty));
}

TEST(CSSDeclaredStyleDataTest, MarginTopSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kMarginTopProperty));

  style->SetPropertyValueAndImportance(kMarginTopProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kMarginTopProperty));
}

TEST(CSSDeclaredStyleDataTest, MaxHeightSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kMaxHeightProperty));

  style->SetPropertyValueAndImportance(kMaxHeightProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kMaxHeightProperty));
}

TEST(CSSDeclaredStyleDataTest, MaxWidthSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kMaxWidthProperty));

  style->SetPropertyValueAndImportance(kMaxWidthProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kMaxWidthProperty));
}

TEST(CSSDeclaredStyleDataTest, MinHeightSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kMinHeightProperty));

  style->SetPropertyValueAndImportance(kMinHeightProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kMinHeightProperty));
}

TEST(CSSDeclaredStyleDataTest, MinWidthSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kMinWidthProperty));

  style->SetPropertyValueAndImportance(kMinWidthProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kMinWidthProperty));
}

TEST(CSSDeclaredStyleDataTest, OpacitySettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kOpacityProperty));

  style->SetPropertyValueAndImportance(kOpacityProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kOpacityProperty));
}

TEST(CSSDeclaredStyleDataTest, OutlineColorSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kOutlineColorProperty));

  style->SetPropertyValueAndImportance(kOutlineColorProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kOutlineColorProperty));
}

TEST(CSSDeclaredStyleDataTest, OutlineStyleSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kOutlineStyleProperty));

  style->SetPropertyValueAndImportance(kOutlineStyleProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kOutlineStyleProperty));
}

TEST(CSSDeclaredStyleDataTest, OutlineWidthSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kOutlineWidthProperty));

  style->SetPropertyValueAndImportance(kOutlineWidthProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kOutlineWidthProperty));
}

TEST(CSSDeclaredStyleDataTest, OverflowSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kOverflowProperty));

  style->SetPropertyValueAndImportance(kOverflowProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kOverflowProperty));
}

TEST(CSSDeclaredStyleDataTest, OverflowWrapSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kOverflowWrapProperty));

  style->SetPropertyValueAndImportance(kOverflowWrapProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kOverflowWrapProperty));
}

TEST(CSSDeclaredStyleDataTest, PaddingBottomSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kPaddingBottomProperty));

  style->SetPropertyValueAndImportance(kPaddingBottomProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kPaddingBottomProperty));
}

TEST(CSSDeclaredStyleDataTest, PaddingLeftSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kPaddingLeftProperty));

  style->SetPropertyValueAndImportance(kPaddingLeftProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kPaddingLeftProperty));
}

TEST(CSSDeclaredStyleDataTest, PaddingRightSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kPaddingRightProperty));

  style->SetPropertyValueAndImportance(kPaddingRightProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kPaddingRightProperty));
}

TEST(CSSDeclaredStyleDataTest, PaddingTopSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kPaddingTopProperty));

  style->SetPropertyValueAndImportance(kPaddingTopProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kPaddingTopProperty));
}

TEST(CSSDeclaredStyleDataTest, PointerEventsSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kPointerEventsProperty));

  style->SetPropertyValueAndImportance(kPointerEventsProperty,
                                       KeywordValue::GetInitial(), false);
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kPointerEventsProperty));

  style->SetPropertyValueAndImportance(kPointerEventsProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kPointerEventsProperty));
}

TEST(CSSDeclaredStyleDataTest, PositionSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kPositionProperty));

  style->SetPropertyValueAndImportance(kPositionProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kPositionProperty));
}

TEST(CSSDeclaredStyleDataTest, TextAlignSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kTextAlignProperty));

  style->SetPropertyValueAndImportance(kTextAlignProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kTextAlignProperty));
}

TEST(CSSDeclaredStyleDataTest,
     TextDecorationColorSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kTextDecorationColorProperty));

  style->SetPropertyValueAndImportance(kTextDecorationColorProperty,
                                       KeywordValue::GetInitial(), false);
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kTextDecorationColorProperty));

  style->SetPropertyValueAndImportance(kTextDecorationColorProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kTextDecorationColorProperty));
}

TEST(CSSDeclaredStyleDataTest,
     TextDecorationLineSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kTextDecorationLineProperty));

  style->SetPropertyValueAndImportance(kTextDecorationLineProperty,
                                       KeywordValue::GetInitial(), false);
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kTextDecorationLineProperty));

  style->SetPropertyValueAndImportance(kTextDecorationLineProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kTextDecorationLineProperty));
}

TEST(CSSDeclaredStyleDataTest, TextIndentSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kTextIndentProperty));

  style->SetPropertyValueAndImportance(kTextIndentProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kTextIndentProperty));
}

TEST(CSSDeclaredStyleDataTest, TextOverflowSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kTextOverflowProperty));

  style->SetPropertyValueAndImportance(kTextOverflowProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kTextOverflowProperty));
}

TEST(CSSDeclaredStyleDataTest, TextTransformSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kTextTransformProperty));

  style->SetPropertyValueAndImportance(kTextTransformProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kTextTransformProperty));
}

TEST(CSSDeclaredStyleDataTest, TransformSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kTransformProperty));

  style->SetPropertyValueAndImportance(kTransformProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kTransformProperty));
}

TEST(CSSDeclaredStyleDataTest, TransitionDelaySettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kTransitionDelayProperty));

  style->SetPropertyValueAndImportance(kTransitionDelayProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kTransitionDelayProperty));
}

TEST(CSSDeclaredStyleDataTest,
     TransitionDurationSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kTransitionDurationProperty));

  style->SetPropertyValueAndImportance(kTransitionDurationProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kTransitionDurationProperty));
}

TEST(CSSDeclaredStyleDataTest,
     TransitionPropertySettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kTransitionPropertyProperty));

  style->SetPropertyValueAndImportance(kTransitionPropertyProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kTransitionPropertyProperty));
}

TEST(CSSDeclaredStyleDataTest,
     TransitionTimingFunctionSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kTransitionTimingFunctionProperty));

  style->SetPropertyValueAndImportance(kTransitionTimingFunctionProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kTransitionTimingFunctionProperty));
}

TEST(CSSDeclaredStyleDataTest, VerticalAlignSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kVerticalAlignProperty));

  style->SetPropertyValueAndImportance(kVerticalAlignProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kVerticalAlignProperty));
}

TEST(CSSDeclaredStyleDataTest, VisibilitySettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kVisibilityProperty));

  style->SetPropertyValueAndImportance(kVisibilityProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kVisibilityProperty));
}

TEST(CSSDeclaredStyleDataTest, WhiteSpaceSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kWhiteSpaceProperty));

  style->SetPropertyValueAndImportance(kWhiteSpaceProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kWhiteSpaceProperty));
}

TEST(CSSDeclaredStyleDataTest, WidthSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kWidthProperty));

  style->SetPropertyValueAndImportance(kWidthProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kWidthProperty));
}

TEST(CSSDeclaredStyleDataTest, ZIndexSettersAndGettersAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->GetPropertyValue(kZIndexProperty));

  style->SetPropertyValueAndImportance(kZIndexProperty,
                                       KeywordValue::GetInherit(), false);
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kZIndexProperty));
}

TEST(CSSDeclaredStyleDataTest,
     PropertyValueSetterAndImportanceGetterAreConsistent) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();

  EXPECT_FALSE(style->IsDeclaredPropertyImportant(kBackgroundColorProperty));
  EXPECT_FALSE(style->IsDeclaredPropertyImportant(kVerticalAlignProperty));

  style->SetPropertyValueAndImportance(kBackgroundColorProperty,
                                       new RGBAColorValue(0x000000ff), true);
  style->SetPropertyValueAndImportance(kVerticalAlignProperty,
                                       KeywordValue::GetBaseline(), false);

  EXPECT_TRUE(style->IsDeclaredPropertyImportant(kBackgroundColorProperty));
  EXPECT_FALSE(style->IsDeclaredPropertyImportant(kVerticalAlignProperty));
}

TEST(CSSDeclaredStyleDataTest, TwoDeclaredStyleDataWithSamePropertiesAreEqual) {
  scoped_refptr<CSSDeclaredStyleData> style1 = new CSSDeclaredStyleData();
  style1->SetPropertyValueAndImportance(kBorderLeftWidthProperty,
                                        KeywordValue::GetInherit(), false);
  scoped_refptr<CSSDeclaredStyleData> style2 = new CSSDeclaredStyleData();
  style2->SetPropertyValueAndImportance(kBorderLeftWidthProperty,
                                        KeywordValue::GetInherit(), false);

  EXPECT_EQ(*style1, *style2);
}

TEST(CSSDeclaredStyleDataTest,
     TwoDeclaredStyleDataWithDifferentImportanceAreUnequal) {
  scoped_refptr<CSSDeclaredStyleData> style1 = new CSSDeclaredStyleData();
  style1->SetPropertyValueAndImportance(kBorderLeftWidthProperty,
                                        KeywordValue::GetInherit(), false);
  scoped_refptr<CSSDeclaredStyleData> style2 = new CSSDeclaredStyleData();
  style2->SetPropertyValueAndImportance(kBorderLeftWidthProperty,
                                        KeywordValue::GetInherit(), true);

  EXPECT_EQ(*style1 == *style2, false);
}

TEST(CSSDeclaredStyleDataTest,
     TwoDeclaredStyleDataWithDifferentPropertiesAreUnequal) {
  scoped_refptr<CSSDeclaredStyleData> style1 = new CSSDeclaredStyleData();
  style1->SetPropertyValueAndImportance(kBorderLeftWidthProperty,
                                        KeywordValue::GetInherit(), true);
  scoped_refptr<CSSDeclaredStyleData> style2 = new CSSDeclaredStyleData();
  style2->SetPropertyValueAndImportance(kBorderRightWidthProperty,
                                        KeywordValue::GetInherit(), true);

  EXPECT_EQ(*style1 == *style2, false);
}

TEST(CSSDeclaredStyleDataTest,
     DeclaredStyleDataIsUnequalToDeclaredStyleDataWithPropertySuperset) {
  scoped_refptr<CSSDeclaredStyleData> style1 = new CSSDeclaredStyleData();
  style1->SetPropertyValueAndImportance(kBorderLeftWidthProperty,
                                        KeywordValue::GetInherit(), true);
  scoped_refptr<CSSDeclaredStyleData> style2 = new CSSDeclaredStyleData();
  style2->SetPropertyValueAndImportance(kBorderLeftWidthProperty,
                                        KeywordValue::GetInherit(), true);
  style2->SetPropertyValueAndImportance(kBorderRightWidthProperty,
                                        KeywordValue::GetInherit(), true);

  EXPECT_EQ(*style1 == *style2, false);
}

TEST(CSSDeclaredStyleDataTest,
     DeclaredStyleDataIsUnequalToDeclaredStyleDataWithPropertySubset) {
  scoped_refptr<CSSDeclaredStyleData> style1 = new CSSDeclaredStyleData();
  style1->SetPropertyValueAndImportance(kBorderLeftWidthProperty,
                                        KeywordValue::GetInherit(), true);
  style1->SetPropertyValueAndImportance(kBorderRightWidthProperty,
                                        KeywordValue::GetInherit(), true);
  scoped_refptr<CSSDeclaredStyleData> style2 = new CSSDeclaredStyleData();
  style2->SetPropertyValueAndImportance(kBorderLeftWidthProperty,
                                        KeywordValue::GetInherit(), true);

  EXPECT_EQ(*style1 == *style2, false);
}

TEST(CSSDeclaredStyleDataTest, CanClearLongHandPropertyValueAndImportance) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();
  style->SetPropertyValueAndImportance(kBorderLeftWidthProperty,
                                       KeywordValue::GetInherit(), true);
  style->ClearPropertyValueAndImportance(kBorderLeftWidthProperty);

  scoped_refptr<CSSDeclaredStyleData> empty_style = new CSSDeclaredStyleData();
  EXPECT_EQ(*style, *empty_style);
}

TEST(CSSDeclaredStyleDataTest, CanClearPropertyValueAndImportance) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();
  style->SetPropertyValueAndImportance(kBorderLeftWidthProperty,
                                       KeywordValue::GetInherit(), true);
  style->ClearPropertyValueAndImportance(kBorderWidthProperty);

  scoped_refptr<CSSDeclaredStyleData> empty_style = new CSSDeclaredStyleData();
  EXPECT_EQ(*style, *empty_style);
}

TEST(CSSDeclaredStyleDataTest,
     CanClearPropertyValueAndImportanceWithRecursion) {
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();
  style->SetPropertyValueAndImportance(kBorderLeftWidthProperty,
                                       KeywordValue::GetInherit(), true);
  style->ClearPropertyValueAndImportance(kBorderProperty);

  scoped_refptr<CSSDeclaredStyleData> empty_style = new CSSDeclaredStyleData();
  EXPECT_EQ(*style, *empty_style);
}

}  // namespace cssom
}  // namespace cobalt
