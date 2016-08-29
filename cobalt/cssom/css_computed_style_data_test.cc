/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/cssom/css_computed_style_data.h"

#include "cobalt/cssom/css_style_sheet.h"
#include "cobalt/cssom/font_weight_value.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/property_definitions.h"
#include "cobalt/cssom/rgba_color_value.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace cssom {

TEST(CSSComputedStyleDataTest, BackgroundColorSettersAndGettersAreConsistent) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kBackgroundColorProperty),
            style->background_color());
  EXPECT_EQ(style->background_color(),
            style->GetPropertyValue(kBackgroundColorProperty));

  style->set_background_color(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->background_color());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kBackgroundColorProperty));

  style->SetPropertyValue(kBackgroundColorProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->background_color());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kBackgroundColorProperty));
}

TEST(CSSComputedStyleDataTest, BackgroundImageSettersAndGettersAreConsistent) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kBackgroundImageProperty),
            style->background_image());
  EXPECT_EQ(style->background_image(),
            style->GetPropertyValue(kBackgroundImageProperty));

  style->set_background_image(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->background_image());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kBackgroundImageProperty));

  style->SetPropertyValue(kBackgroundImageProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->background_image());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kBackgroundImageProperty));
}

TEST(CSSComputedStyleDataTest,
     BackgroundPositionSettersAndGettersAreConsistent) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kBackgroundPositionProperty),
            style->background_position());
  EXPECT_EQ(style->background_position(),
            style->GetPropertyValue(kBackgroundPositionProperty));

  style->set_background_position(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->background_position());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kBackgroundPositionProperty));

  style->SetPropertyValue(kBackgroundPositionProperty,
                          KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->background_position());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kBackgroundPositionProperty));
}

TEST(CSSComputedStyleDataTest, BackgroundRepeatSettersAndGettersAreConsistent) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kBackgroundRepeatProperty),
            style->background_repeat());
  EXPECT_EQ(style->background_repeat(),
            style->GetPropertyValue(kBackgroundRepeatProperty));

  style->set_background_repeat(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->background_repeat());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kBackgroundRepeatProperty));

  style->SetPropertyValue(kBackgroundRepeatProperty,
                          KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->background_repeat());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kBackgroundRepeatProperty));
}

TEST(CSSComputedStyleDataTest, BackgroundSizeSettersAndGettersAreConsistent) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kBackgroundSizeProperty),
            style->background_size());
  EXPECT_EQ(style->background_size(),
            style->GetPropertyValue(kBackgroundSizeProperty));

  style->set_background_size(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->background_size());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kBackgroundSizeProperty));

  style->SetPropertyValue(kBackgroundSizeProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->background_size());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kBackgroundSizeProperty));
}

TEST(CSSComputedStyleDataTest, BorderRadiusSettersAndGettersAreConsistent) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kBorderRadiusProperty),
            style->border_radius());
  EXPECT_EQ(style->border_radius(),
            style->GetPropertyValue(kBorderRadiusProperty));

  style->set_border_radius(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->border_radius());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kBorderRadiusProperty));

  style->SetPropertyValue(kBorderRadiusProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->border_radius());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kBorderRadiusProperty));
}

TEST(CSSComputedStyleDataTest, ColorSettersAndGettersAreConsistent) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kColorProperty), style->color());
  EXPECT_EQ(style->color(), style->GetPropertyValue(kColorProperty));

  style->set_color(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->color());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kColorProperty));

  style->SetPropertyValue(kColorProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->color());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kColorProperty));
}

TEST(CSSComputedStyleDataTest, ContentSettersAndGettersAreConsistent) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kContentProperty), style->content());
  EXPECT_EQ(style->content(), style->GetPropertyValue(kContentProperty));

  style->set_content(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->content());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kContentProperty));

  style->SetPropertyValue(kContentProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->content());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kContentProperty));
}

TEST(CSSComputedStyleDataTest, DisplaySettersAndGettersAreConsistent) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kDisplayProperty), style->display());
  EXPECT_EQ(style->display(), style->GetPropertyValue(kDisplayProperty));

  style->set_display(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->display());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kDisplayProperty));

  style->SetPropertyValue(kDisplayProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->display());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kDisplayProperty));
}

TEST(CSSComputedStyleDataTest, FontFamilySettersAndGettersAreConsistent) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kFontFamilyProperty), style->font_family());
  EXPECT_EQ(style->font_family(), style->GetPropertyValue(kFontFamilyProperty));

  style->set_font_family(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->font_family());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kFontFamilyProperty));

  style->SetPropertyValue(kFontFamilyProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->font_family());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kFontFamilyProperty));
}

TEST(CSSComputedStyleDataTest, FontSizeSettersAndGettersAreConsistent) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kFontSizeProperty), style->font_size());
  EXPECT_EQ(style->font_size(), style->GetPropertyValue(kFontSizeProperty));

  style->set_font_size(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->font_size());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kFontSizeProperty));

  style->SetPropertyValue(kFontSizeProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->font_size());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kFontSizeProperty));
}

TEST(CSSComputedStyleDataTest, FontStyleSettersAndGettersAreConsistent) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kFontStyleProperty), style->font_style());
  EXPECT_EQ(style->font_style(), style->GetPropertyValue(kFontStyleProperty));

  style->set_font_style(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->font_style());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kFontStyleProperty));

  style->SetPropertyValue(kFontStyleProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->font_style());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kFontStyleProperty));
}

TEST(CSSComputedStyleDataTest, FontWeightSettersAndGettersAreConsistent) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kFontWeightProperty), style->font_weight());
  EXPECT_EQ(style->font_weight(), style->GetPropertyValue(kFontWeightProperty));

  style->set_font_weight(FontWeightValue::GetNormalAka400());
  EXPECT_EQ(FontWeightValue::GetNormalAka400(), style->font_weight());
  EXPECT_EQ(FontWeightValue::GetNormalAka400(),
            style->GetPropertyValue(kFontWeightProperty));

  style->SetPropertyValue(kFontWeightProperty,
                          FontWeightValue::GetBoldAka700());
  EXPECT_EQ(FontWeightValue::GetBoldAka700(), style->font_weight());
  EXPECT_EQ(FontWeightValue::GetBoldAka700(),
            style->GetPropertyValue(kFontWeightProperty));
}

TEST(CSSComputedStyleDataTest, HeightSettersAndGettersAreConsistent) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kHeightProperty), style->height());
  EXPECT_EQ(style->height(), style->GetPropertyValue(kHeightProperty));

  style->set_height(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->height());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kHeightProperty));

  style->SetPropertyValue(kHeightProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->height());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kHeightProperty));
}

TEST(CSSComputedStyleDataTest, LineHeightSettersAndGettersAreConsistent) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kLineHeightProperty), style->line_height());
  EXPECT_EQ(style->line_height(), style->GetPropertyValue(kLineHeightProperty));

  style->set_line_height(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->line_height());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kLineHeightProperty));

  style->SetPropertyValue(kLineHeightProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->line_height());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kLineHeightProperty));
}

TEST(CSSComputedStyleDataTest, MarginBottomSettersAndGettersAreConsistent) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kMarginBottomProperty),
            style->margin_bottom());
  EXPECT_EQ(style->margin_bottom(),
            style->GetPropertyValue(kMarginBottomProperty));

  style->set_margin_bottom(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->margin_bottom());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kMarginBottomProperty));

  style->SetPropertyValue(kMarginBottomProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->margin_bottom());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kMarginBottomProperty));
}

TEST(CSSComputedStyleDataTest, MarginLeftSettersAndGettersAreConsistent) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kMarginLeftProperty), style->margin_left());
  EXPECT_EQ(style->margin_left(), style->GetPropertyValue(kMarginLeftProperty));

  style->set_margin_left(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->margin_left());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kMarginLeftProperty));

  style->SetPropertyValue(kMarginLeftProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->margin_left());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kMarginLeftProperty));
}

TEST(CSSComputedStyleDataTest, MarginRightSettersAndGettersAreConsistent) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kMarginRightProperty),
            style->margin_right());
  EXPECT_EQ(style->margin_right(),
            style->GetPropertyValue(kMarginRightProperty));

  style->set_margin_right(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->margin_right());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kMarginRightProperty));

  style->SetPropertyValue(kMarginRightProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->margin_right());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kMarginRightProperty));
}

TEST(CSSComputedStyleDataTest, MarginTopSettersAndGettersAreConsistent) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kMarginTopProperty), style->margin_top());
  EXPECT_EQ(style->margin_top(), style->GetPropertyValue(kMarginTopProperty));

  style->set_margin_top(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->margin_top());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kMarginTopProperty));

  style->SetPropertyValue(kMarginTopProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->margin_top());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kMarginTopProperty));
}

TEST(CSSComputedStyleDataTest, MaxHeightSettersAndGettersAreConsistent) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kMaxHeightProperty), style->max_height());
  EXPECT_EQ(style->max_height(), style->GetPropertyValue(kMaxHeightProperty));

  style->set_max_height(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->max_height());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kMaxHeightProperty));

  style->SetPropertyValue(kMaxHeightProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->max_height());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kMaxHeightProperty));
}

TEST(CSSComputedStyleDataTest, MaxWidthSettersAndGettersAreConsistent) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kMaxWidthProperty), style->max_width());
  EXPECT_EQ(style->max_width(), style->GetPropertyValue(kMaxWidthProperty));

  style->set_max_width(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->max_width());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kMaxWidthProperty));

  style->SetPropertyValue(kMaxWidthProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->max_width());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kMaxWidthProperty));
}

TEST(CSSComputedStyleDataTest, MinHeightSettersAndGettersAreConsistent) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kMinHeightProperty), style->min_height());
  EXPECT_EQ(style->min_height(), style->GetPropertyValue(kMinHeightProperty));

  style->set_min_height(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->min_height());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kMinHeightProperty));

  style->SetPropertyValue(kMinHeightProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->min_height());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kMinHeightProperty));
}

TEST(CSSComputedStyleDataTest, MinWidthSettersAndGettersAreConsistent) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kMinWidthProperty), style->min_width());
  EXPECT_EQ(style->min_width(), style->GetPropertyValue(kMinWidthProperty));

  style->set_min_width(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->min_width());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kMinWidthProperty));

  style->SetPropertyValue(kMinWidthProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->min_width());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kMinWidthProperty));
}

TEST(CSSComputedStyleDataTest, OpacitySettersAndGettersAreConsistent) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kOpacityProperty), style->opacity());
  EXPECT_EQ(style->opacity(), style->GetPropertyValue(kOpacityProperty));

  style->set_opacity(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->opacity());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kOpacityProperty));

  style->SetPropertyValue(kOpacityProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->opacity());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kOpacityProperty));
}

TEST(CSSComputedStyleDataTest, OverflowSettersAndGettersAreConsistent) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kOverflowProperty), style->overflow());
  EXPECT_EQ(style->overflow(), style->GetPropertyValue(kOverflowProperty));

  style->set_overflow(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->overflow());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kOverflowProperty));

  style->SetPropertyValue(kOverflowProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->overflow());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kOverflowProperty));
}

TEST(CSSComputedStyleDataTest, OverflowWrapSettersAndGettersAreConsistent) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kOverflowWrapProperty),
            style->overflow_wrap());
  EXPECT_EQ(style->overflow_wrap(),
            style->GetPropertyValue(kOverflowWrapProperty));

  style->set_overflow_wrap(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->overflow_wrap());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kOverflowWrapProperty));

  style->SetPropertyValue(kOverflowWrapProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->overflow_wrap());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kOverflowWrapProperty));
}

TEST(CSSComputedStyleDataTest, PaddingBottomSettersAndGettersAreConsistent) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kPaddingBottomProperty),
            style->padding_bottom());
  EXPECT_EQ(style->padding_bottom(),
            style->GetPropertyValue(kPaddingBottomProperty));

  style->set_padding_bottom(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->padding_bottom());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kPaddingBottomProperty));

  style->SetPropertyValue(kPaddingBottomProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->padding_bottom());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kPaddingBottomProperty));
}

TEST(CSSComputedStyleDataTest, PaddingLeftSettersAndGettersAreConsistent) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kPaddingLeftProperty),
            style->padding_left());
  EXPECT_EQ(style->padding_left(),
            style->GetPropertyValue(kPaddingLeftProperty));

  style->set_padding_left(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->padding_left());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kPaddingLeftProperty));

  style->SetPropertyValue(kPaddingLeftProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->padding_left());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kPaddingLeftProperty));
}

TEST(CSSComputedStyleDataTest, PaddingRightSettersAndGettersAreConsistent) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kPaddingRightProperty),
            style->padding_right());
  EXPECT_EQ(style->padding_right(),
            style->GetPropertyValue(kPaddingRightProperty));

  style->set_padding_right(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->padding_right());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kPaddingRightProperty));

  style->SetPropertyValue(kPaddingRightProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->padding_right());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kPaddingRightProperty));
}

TEST(CSSComputedStyleDataTest, PaddingTopSettersAndGettersAreConsistent) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kPaddingTopProperty), style->padding_top());
  EXPECT_EQ(style->padding_top(), style->GetPropertyValue(kPaddingTopProperty));

  style->set_padding_top(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->padding_top());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kPaddingTopProperty));

  style->SetPropertyValue(kPaddingTopProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->padding_top());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kPaddingTopProperty));
}

TEST(CSSComputedStyleDataTest, PositionSettersAndGettersAreConsistent) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kPositionProperty), style->position());
  EXPECT_EQ(style->position(), style->GetPropertyValue(kPositionProperty));

  style->set_position(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->position());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kPositionProperty));

  style->SetPropertyValue(kPositionProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->position());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kPositionProperty));
}

TEST(CSSComputedStyleDataTest, TextAlignSettersAndGettersAreConsistent) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kTextAlignProperty), style->text_align());
  EXPECT_EQ(style->text_align(), style->GetPropertyValue(kTextAlignProperty));

  style->set_text_align(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->text_align());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kTextAlignProperty));

  style->SetPropertyValue(kTextAlignProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->text_align());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kTextAlignProperty));
}

TEST(CSSComputedStyleDataTest,
     TextDecorationColorSettersAndGettersAreConsistent) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kTextDecorationColorProperty),
            KeywordValue::GetCurrentColor());
  EXPECT_EQ(style->text_decoration_color(),
            style->GetPropertyValue(kTextDecorationColorProperty));

  style->set_text_decoration_color(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->text_decoration_color());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kTextDecorationColorProperty));

  style->SetPropertyValue(kTextDecorationColorProperty,
                          KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->text_decoration_color());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kTextDecorationColorProperty));
}

TEST(CSSComputedStyleDataTest,
     TextDecorationLineSettersAndGettersAreConsistent) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kTextDecorationLineProperty),
            style->text_decoration_line());
  EXPECT_EQ(style->text_decoration_line(),
            style->GetPropertyValue(kTextDecorationLineProperty));

  style->set_text_decoration_line(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->text_decoration_line());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kTextDecorationLineProperty));

  style->SetPropertyValue(kTextDecorationLineProperty,
                          KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->text_decoration_line());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kTextDecorationLineProperty));
}

TEST(CSSComputedStyleDataTest, TextIndentSettersAndGettersAreConsistent) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kTextIndentProperty), style->text_indent());
  EXPECT_EQ(style->text_indent(), style->GetPropertyValue(kTextIndentProperty));

  style->set_text_indent(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->text_indent());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kTextIndentProperty));

  style->SetPropertyValue(kTextIndentProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->text_indent());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kTextIndentProperty));
}

TEST(CSSComputedStyleDataTest, TextOverflowSettersAndGettersAreConsistent) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kTextOverflowProperty),
            style->text_overflow());
  EXPECT_EQ(style->text_overflow(),
            style->GetPropertyValue(kTextOverflowProperty));

  style->set_text_overflow(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->text_overflow());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kTextOverflowProperty));

  style->SetPropertyValue(kTextOverflowProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->text_overflow());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kTextOverflowProperty));
}

TEST(CSSComputedStyleDataTest, TextTransformSettersAndGettersAreConsistent) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kTextTransformProperty),
            style->text_transform());
  EXPECT_EQ(style->text_transform(),
            style->GetPropertyValue(kTextTransformProperty));

  style->set_text_transform(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->text_transform());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kTextTransformProperty));

  style->SetPropertyValue(kTextTransformProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->text_transform());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kTextTransformProperty));
}

TEST(CSSComputedStyleDataTest, TransformSettersAndGettersAreConsistent) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kTransformProperty), style->transform());
  EXPECT_EQ(style->transform(), style->GetPropertyValue(kTransformProperty));

  style->set_transform(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->transform());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kTransformProperty));

  style->SetPropertyValue(kTransformProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->transform());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kTransformProperty));
}

TEST(CSSComputedStyleDataTest, TransitionDelaySettersAndGettersAreConsistent) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kTransitionDelayProperty),
            style->transition_delay());
  EXPECT_EQ(style->transition_delay(),
            style->GetPropertyValue(kTransitionDelayProperty));

  style->set_transition_delay(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->transition_delay());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kTransitionDelayProperty));

  style->SetPropertyValue(kTransitionDelayProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->transition_delay());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kTransitionDelayProperty));
}

TEST(CSSComputedStyleDataTest,
     TransitionDurationSettersAndGettersAreConsistent) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kTransitionDurationProperty),
            style->transition_duration());
  EXPECT_EQ(style->transition_duration(),
            style->GetPropertyValue(kTransitionDurationProperty));

  style->set_transition_duration(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->transition_duration());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kTransitionDurationProperty));

  style->SetPropertyValue(kTransitionDurationProperty,
                          KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->transition_duration());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kTransitionDurationProperty));
}

TEST(CSSComputedStyleDataTest,
     TransitionPropertySettersAndGettersAreConsistent) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kTransitionPropertyProperty),
            style->transition_property());
  EXPECT_EQ(style->transition_property(),
            style->GetPropertyValue(kTransitionPropertyProperty));

  style->set_transition_property(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->transition_property());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kTransitionPropertyProperty));

  style->SetPropertyValue(kTransitionPropertyProperty,
                          KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->transition_property());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kTransitionPropertyProperty));
}

TEST(CSSComputedStyleDataTest,
     TransitionTimingFunctionSettersAndGettersAreConsistent) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kTransitionTimingFunctionProperty),
            style->transition_timing_function());
  EXPECT_EQ(style->transition_timing_function(),
            style->GetPropertyValue(kTransitionTimingFunctionProperty));

  style->set_transition_timing_function(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->transition_timing_function());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kTransitionTimingFunctionProperty));

  style->SetPropertyValue(kTransitionTimingFunctionProperty,
                          KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->transition_timing_function());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kTransitionTimingFunctionProperty));
}

TEST(CSSComputedStyleDataTest, VerticalAlignSettersAndGettersAreConsistent) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kVerticalAlignProperty),
            style->vertical_align());
  EXPECT_EQ(style->vertical_align(),
            style->GetPropertyValue(kVerticalAlignProperty));

  style->set_vertical_align(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->vertical_align());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kVerticalAlignProperty));

  style->SetPropertyValue(kVerticalAlignProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->vertical_align());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kVerticalAlignProperty));
}

TEST(CSSComputedStyleDataTest, VisibilitySettersAndGettersAreConsistent) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kVisibilityProperty), style->visibility());
  EXPECT_EQ(style->visibility(), style->GetPropertyValue(kVisibilityProperty));

  style->set_visibility(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->visibility());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kVisibilityProperty));

  style->SetPropertyValue(kVisibilityProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->visibility());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kVisibilityProperty));
}

TEST(CSSComputedStyleDataTest, WhiteSpaceSettersAndGettersAreConsistent) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kWhiteSpaceProperty), style->white_space());
  EXPECT_EQ(style->white_space(), style->GetPropertyValue(kWhiteSpaceProperty));

  style->set_white_space(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->white_space());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kWhiteSpaceProperty));

  style->SetPropertyValue(kWhiteSpaceProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->white_space());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kWhiteSpaceProperty));
}

TEST(CSSComputedStyleDataTest, WidthSettersAndGettersAreConsistent) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kWidthProperty), style->width());
  EXPECT_EQ(style->width(), style->GetPropertyValue(kWidthProperty));

  style->set_width(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->width());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kWidthProperty));

  style->SetPropertyValue(kWidthProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->width());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kWidthProperty));
}

TEST(CSSComputedStyleDataTest, ZIndexSettersAndGettersAreConsistent) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kZIndexProperty), style->z_index());
  EXPECT_EQ(style->z_index(), style->GetPropertyValue(kZIndexProperty));

  style->set_z_index(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->z_index());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kZIndexProperty));

  style->SetPropertyValue(kZIndexProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->z_index());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kZIndexProperty));
}

TEST(CSSComputedStyleDataTest,
     BorderTopColorComputedInitialValueIsSameAsColor) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kBorderTopColorProperty),
            KeywordValue::GetCurrentColor());
  EXPECT_EQ(style->border_top_color(),
            style->GetPropertyValue(kBorderTopColorProperty));

  style->set_color(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->border_top_color());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kBorderTopColorProperty));

  style->SetPropertyValue(kBorderTopColorProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->border_top_color());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kBorderTopColorProperty));
}

TEST(CSSComputedStyleDataTest,
     BorderRightColorComputedInitialValueIsSameAsColor) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kBorderRightColorProperty),
            KeywordValue::GetCurrentColor());
  EXPECT_EQ(style->border_right_color(),
            style->GetPropertyValue(kBorderRightColorProperty));

  style->set_color(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->border_right_color());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kBorderRightColorProperty));

  style->SetPropertyValue(kBorderRightColorProperty,
                          KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->border_right_color());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kBorderRightColorProperty));
}

TEST(CSSComputedStyleDataTest,
     BorderBottomColorComputedInitialValueIsSameAsColor) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kBorderBottomColorProperty),
            KeywordValue::GetCurrentColor());
  EXPECT_EQ(style->border_bottom_color(),
            style->GetPropertyValue(kBorderBottomColorProperty));

  style->set_color(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->border_bottom_color());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kBorderBottomColorProperty));

  style->SetPropertyValue(kBorderBottomColorProperty,
                          KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->border_bottom_color());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kBorderBottomColorProperty));
}

TEST(CSSComputedStyleDataTest,
     BorderLeftColorComputedInitialValueIsSameAsColor) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kBorderLeftColorProperty),
            KeywordValue::GetCurrentColor());
  EXPECT_EQ(style->border_left_color(),
            style->GetPropertyValue(kBorderLeftColorProperty));

  style->set_color(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->border_left_color());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kBorderLeftColorProperty));

  style->SetPropertyValue(kBorderLeftColorProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->border_left_color());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kBorderLeftColorProperty));
}

TEST(CSSComputedStyleDataTest, BorderTopWidthIsZeroWhenStyleIsNoneOrHidden) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  EXPECT_EQ(style->border_top_width(),
            style->GetPropertyValue(kBorderTopWidthProperty));

  style->set_border_top_style(KeywordValue::GetSolid());
  EXPECT_EQ(GetPropertyInitialValue(kBorderTopWidthProperty),
            style->border_top_width());

  style->set_border_top_style(KeywordValue::GetNone());
  LengthValue* length_value =
      dynamic_cast<LengthValue*>(style->border_top_width().get());
  ASSERT_TRUE(length_value);
  EXPECT_EQ(0.0f, length_value->value());
  EXPECT_EQ(kPixelsUnit, length_value->unit());
  EXPECT_NE(GetPropertyInitialValue(kBorderTopWidthProperty),
            style->border_top_width());

  style->set_border_top_style(KeywordValue::GetSolid());
  EXPECT_EQ(GetPropertyInitialValue(kBorderTopWidthProperty),
            style->border_top_width());

  style->set_border_top_style(KeywordValue::GetHidden());
  length_value = dynamic_cast<LengthValue*>(style->border_top_width().get());
  ASSERT_TRUE(length_value);
  EXPECT_EQ(0.0f, length_value->value());
  EXPECT_EQ(kPixelsUnit, length_value->unit());
  EXPECT_NE(GetPropertyInitialValue(kBorderTopWidthProperty),
            style->border_top_width());

  style->SetPropertyValue(kBorderTopWidthProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->border_top_width());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kBorderTopWidthProperty));
}

TEST(CSSComputedStyleDataTest, BorderRightWidthIsZeroWhenStyleIsNoneOrHidden) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  EXPECT_EQ(style->border_right_width(),
            style->GetPropertyValue(kBorderRightWidthProperty));

  style->set_border_right_style(KeywordValue::GetSolid());
  EXPECT_EQ(GetPropertyInitialValue(kBorderRightWidthProperty),
            style->border_right_width());

  style->set_border_right_style(KeywordValue::GetNone());
  LengthValue* length_value =
      dynamic_cast<LengthValue*>(style->border_right_width().get());
  ASSERT_TRUE(length_value);
  EXPECT_EQ(0.0f, length_value->value());
  EXPECT_EQ(kPixelsUnit, length_value->unit());
  EXPECT_NE(GetPropertyInitialValue(kBorderRightWidthProperty),
            style->border_right_width());

  style->set_border_right_style(KeywordValue::GetSolid());
  EXPECT_EQ(GetPropertyInitialValue(kBorderRightWidthProperty),
            style->border_right_width());

  style->set_border_right_style(KeywordValue::GetHidden());
  length_value = dynamic_cast<LengthValue*>(style->border_right_width().get());
  ASSERT_TRUE(length_value);
  EXPECT_EQ(0.0f, length_value->value());
  EXPECT_EQ(kPixelsUnit, length_value->unit());
  EXPECT_NE(GetPropertyInitialValue(kBorderRightWidthProperty),
            style->border_right_width());

  style->SetPropertyValue(kBorderRightWidthProperty,
                          KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->border_right_width());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kBorderRightWidthProperty));
}

TEST(CSSComputedStyleDataTest, BorderBottomWidthIsZeroWhenStyleIsNoneOrHidden) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  EXPECT_EQ(style->border_bottom_width(),
            style->GetPropertyValue(kBorderBottomWidthProperty));

  style->set_border_bottom_style(KeywordValue::GetSolid());
  EXPECT_EQ(GetPropertyInitialValue(kBorderBottomWidthProperty),
            style->border_bottom_width());

  style->set_border_bottom_style(KeywordValue::GetNone());
  LengthValue* length_value =
      dynamic_cast<LengthValue*>(style->border_bottom_width().get());
  ASSERT_TRUE(length_value);
  EXPECT_EQ(0.0f, length_value->value());
  EXPECT_EQ(kPixelsUnit, length_value->unit());
  EXPECT_NE(GetPropertyInitialValue(kBorderBottomWidthProperty),
            style->border_bottom_width());

  style->set_border_bottom_style(KeywordValue::GetSolid());
  EXPECT_EQ(GetPropertyInitialValue(kBorderBottomWidthProperty),
            style->border_bottom_width());

  style->set_border_bottom_style(KeywordValue::GetHidden());
  length_value = dynamic_cast<LengthValue*>(style->border_bottom_width().get());
  ASSERT_TRUE(length_value);
  EXPECT_EQ(0.0f, length_value->value());
  EXPECT_EQ(kPixelsUnit, length_value->unit());
  EXPECT_NE(GetPropertyInitialValue(kBorderBottomWidthProperty),
            style->border_bottom_width());

  style->SetPropertyValue(kBorderBottomWidthProperty,
                          KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->border_bottom_width());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kBorderBottomWidthProperty));
}

TEST(CSSComputedStyleDataTest, BorderLeftWidthIsZeroWhenStyleIsNoneOrHidden) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  EXPECT_EQ(style->border_left_width(),
            style->GetPropertyValue(kBorderLeftWidthProperty));

  style->set_border_left_style(KeywordValue::GetSolid());
  EXPECT_EQ(GetPropertyInitialValue(kBorderLeftWidthProperty),
            style->border_left_width());

  style->set_border_left_style(KeywordValue::GetNone());
  LengthValue* length_value =
      dynamic_cast<LengthValue*>(style->border_left_width().get());
  ASSERT_TRUE(length_value);
  EXPECT_EQ(0.0f, length_value->value());
  EXPECT_EQ(kPixelsUnit, length_value->unit());
  EXPECT_NE(GetPropertyInitialValue(kBorderLeftWidthProperty),
            style->border_left_width());

  style->set_border_left_style(KeywordValue::GetSolid());
  EXPECT_EQ(GetPropertyInitialValue(kBorderLeftWidthProperty),
            style->border_left_width());

  style->set_border_left_style(KeywordValue::GetHidden());
  length_value = dynamic_cast<LengthValue*>(style->border_left_width().get());
  ASSERT_TRUE(length_value);
  EXPECT_EQ(0.0f, length_value->value());
  EXPECT_EQ(kPixelsUnit, length_value->unit());
  EXPECT_NE(GetPropertyInitialValue(kBorderLeftWidthProperty),
            style->border_left_width());

  style->SetPropertyValue(kBorderLeftWidthProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->border_left_width());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kBorderLeftWidthProperty));
}

TEST(CSSComputedStyleDataTest, DisplayIsBlockWhenPositionIsAbsoluteOrFixed) {
  scoped_refptr<CSSComputedStyleData> style = new CSSComputedStyleData();

  style->set_position(KeywordValue::GetAbsolute());
  EXPECT_EQ(KeywordValue::GetBlock(), style->display());

  style->set_position(KeywordValue::GetStatic());
  EXPECT_EQ(KeywordValue::GetInline(), style->display());

  style->set_position(KeywordValue::GetFixed());
  EXPECT_EQ(KeywordValue::GetBlock(), style->display());
}

TEST(CSSComputedStyleDataTest,
     DoDeclaredPropertiesMatchWorksWithUnequalNumberOfDeclaredProperties) {
  scoped_refptr<CSSComputedStyleData> style1 = new CSSComputedStyleData();
  style1->set_font_size(new LengthValue(50, kPixelsUnit));

  scoped_refptr<CSSComputedStyleData> style2 = new CSSComputedStyleData();

  ASSERT_FALSE(style1->DoDeclaredPropertiesMatch(style2));
  ASSERT_FALSE(style2->DoDeclaredPropertiesMatch(style1));
}

TEST(CSSComputedStyleDataTest,
     DoDeclaredPropertiesMatchWorksWithSingleUnequalProperty) {
  scoped_refptr<CSSComputedStyleData> style1 = new CSSComputedStyleData();
  style1->set_font_size(new LengthValue(50, kPixelsUnit));

  scoped_refptr<CSSComputedStyleData> style2 = new CSSComputedStyleData();
  style2->set_font_size(new LengthValue(30, kPixelsUnit));

  ASSERT_FALSE(style1->DoDeclaredPropertiesMatch(style2));
  ASSERT_FALSE(style2->DoDeclaredPropertiesMatch(style1));
}

TEST(CSSComputedStyleDataTest,
     DoDeclaredPropertiesMatchWorksWithSingleEqualProperty) {
  scoped_refptr<CSSComputedStyleData> style1 = new CSSComputedStyleData();
  style1->set_font_size(new LengthValue(50, kPixelsUnit));

  scoped_refptr<CSSComputedStyleData> style2 = new CSSComputedStyleData();
  style2->set_font_size(new LengthValue(50, kPixelsUnit));

  ASSERT_TRUE(style1->DoDeclaredPropertiesMatch(style2));
  ASSERT_TRUE(style2->DoDeclaredPropertiesMatch(style1));
}

TEST(CSSComputedStyleDataTest,
     DoDeclaredPropertiesMatchWorksWithMultipleUnequalProperties) {
  scoped_refptr<CSSComputedStyleData> style1 = new CSSComputedStyleData();
  style1->set_position(KeywordValue::GetAbsolute());
  style1->set_font_size(new LengthValue(50, kPixelsUnit));

  scoped_refptr<CSSComputedStyleData> style2 = new CSSComputedStyleData();
  style2->set_position(KeywordValue::GetAbsolute());
  style2->set_font_size(new LengthValue(30, kPixelsUnit));

  ASSERT_FALSE(style1->DoDeclaredPropertiesMatch(style2));
  ASSERT_FALSE(style2->DoDeclaredPropertiesMatch(style1));
}

TEST(CSSComputedStyleDataTest,
     DoDeclaredPropertiesMatchWorksWithMultipleEqualProperty) {
  scoped_refptr<CSSComputedStyleData> style1 = new CSSComputedStyleData();
  style1->set_position(KeywordValue::GetAbsolute());
  style1->set_font_size(new LengthValue(50, kPixelsUnit));

  scoped_refptr<CSSComputedStyleData> style2 = new CSSComputedStyleData();
  style2->set_position(KeywordValue::GetAbsolute());
  style2->set_font_size(new LengthValue(50, kPixelsUnit));

  ASSERT_TRUE(style1->DoDeclaredPropertiesMatch(style2));
  ASSERT_TRUE(style2->DoDeclaredPropertiesMatch(style1));
}

}  // namespace cssom
}  // namespace cobalt
