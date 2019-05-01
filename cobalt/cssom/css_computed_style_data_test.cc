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

#include "cobalt/cssom/css_computed_style_data.h"
#include "cobalt/cssom/css_style_sheet.h"
#include "cobalt/cssom/font_weight_value.h"
#include "cobalt/cssom/integer_value.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/property_definitions.h"
#include "cobalt/cssom/rgba_color_value.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace cssom {

TEST(CSSComputedStyleDataTest, AlignContentSettersAndGettersAreConsistent) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kAlignContentProperty),
            style->align_content());
  EXPECT_EQ(style->align_content(),
            style->GetPropertyValue(kAlignContentProperty));

  style->set_align_content(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->align_content());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kAlignContentProperty));

  style->SetPropertyValue(kAlignContentProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->align_content());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kAlignContentProperty));
}

TEST(CSSComputedStyleDataTest, AlignItemsSettersAndGettersAreConsistent) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kAlignItemsProperty), style->align_items());
  EXPECT_EQ(style->align_items(), style->GetPropertyValue(kAlignItemsProperty));

  style->set_align_items(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->align_items());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kAlignItemsProperty));

  style->SetPropertyValue(kAlignItemsProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->align_items());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kAlignItemsProperty));
}

TEST(CSSComputedStyleDataTest, AlignSelfSettersAndGettersAreConsistent) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kAlignSelfProperty),
            style->GetPropertyValue(kAlignSelfProperty));

  style->set_align_self(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kAlignSelfProperty));

  style->SetPropertyValue(kAlignSelfProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kAlignSelfProperty));
}

TEST(CSSComputedStyleDataTest, AnimationDelaySettersAndGettersAreConsistent) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kAnimationDelayProperty),
            style->animation_delay());
  EXPECT_EQ(style->animation_delay(),
            style->GetPropertyValue(kAnimationDelayProperty));

  style->set_animation_delay(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->animation_delay());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kAnimationDelayProperty));

  style->SetPropertyValue(kAnimationDelayProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->animation_delay());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kAnimationDelayProperty));
}

TEST(CSSComputedStyleDataTest,
     AnimationDirectionSettersAndGettersAreConsistent) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kAnimationDirectionProperty),
            style->animation_direction());
  EXPECT_EQ(style->animation_direction(),
            style->GetPropertyValue(kAnimationDirectionProperty));

  style->set_animation_direction(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->animation_direction());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kAnimationDirectionProperty));

  style->SetPropertyValue(kAnimationDirectionProperty,
                          KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->animation_direction());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kAnimationDirectionProperty));
}

TEST(CSSComputedStyleDataTest,
     AnimationDurationSettersAndGettersAreConsistent) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kAnimationDurationProperty),
            style->animation_duration());
  EXPECT_EQ(style->animation_duration(),
            style->GetPropertyValue(kAnimationDurationProperty));

  style->set_animation_duration(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->animation_duration());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kAnimationDurationProperty));

  style->SetPropertyValue(kAnimationDurationProperty,
                          KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->animation_duration());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kAnimationDurationProperty));
}

TEST(CSSComputedStyleDataTest,
     AnimationFillModeSettersAndGettersAreConsistent) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kAnimationFillModeProperty),
            style->animation_fill_mode());
  EXPECT_EQ(style->animation_fill_mode(),
            style->GetPropertyValue(kAnimationFillModeProperty));

  style->set_animation_fill_mode(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->animation_fill_mode());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kAnimationFillModeProperty));

  style->SetPropertyValue(kAnimationFillModeProperty,
                          KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->animation_fill_mode());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kAnimationFillModeProperty));
}

TEST(CSSComputedStyleDataTest,
     AnimationIterationCountSettersAndGettersAreConsistent) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kAnimationIterationCountProperty),
            style->animation_iteration_count());
  EXPECT_EQ(style->animation_iteration_count(),
            style->GetPropertyValue(kAnimationIterationCountProperty));

  style->set_animation_iteration_count(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->animation_iteration_count());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kAnimationIterationCountProperty));

  style->SetPropertyValue(kAnimationIterationCountProperty,
                          KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->animation_iteration_count());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kAnimationIterationCountProperty));
}

TEST(CSSComputedStyleDataTest, AnimationNameSettersAndGettersAreConsistent) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kAnimationNameProperty),
            style->animation_name());
  EXPECT_EQ(style->animation_name(),
            style->GetPropertyValue(kAnimationNameProperty));

  style->set_animation_name(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->animation_name());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kAnimationNameProperty));

  style->SetPropertyValue(kAnimationNameProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->animation_name());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kAnimationNameProperty));
}

TEST(CSSComputedStyleDataTest,
     AnimationTimingFunctionSettersAndGettersAreConsistent) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kAnimationTimingFunctionProperty),
            style->animation_timing_function());
  EXPECT_EQ(style->animation_timing_function(),
            style->GetPropertyValue(kAnimationTimingFunctionProperty));

  style->set_animation_timing_function(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->animation_timing_function());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kAnimationTimingFunctionProperty));

  style->SetPropertyValue(kAnimationTimingFunctionProperty,
                          KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->animation_timing_function());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kAnimationTimingFunctionProperty));
}

TEST(CSSComputedStyleDataTest, BackgroundColorSettersAndGettersAreConsistent) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

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
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

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
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

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
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

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
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

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

TEST(CSSComputedStyleDataTest, BorderTopColorSettersAndGettersAreConsistent) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

  EXPECT_EQ(style->border_top_color(),
            style->GetPropertyValue(kBorderTopColorProperty));

  style->set_border_top_color(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->border_top_color());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kBorderTopColorProperty));

  style->SetPropertyValue(kBorderTopColorProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->border_top_color());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kBorderTopColorProperty));
}

TEST(CSSComputedStyleDataTest,
     BorderTopColorComputedInitialValueIsSameAsColor) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

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

TEST(CSSComputedStyleDataTest, BorderRightColorSettersAndGettersAreConsistent) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

  EXPECT_EQ(style->border_right_color(),
            style->GetPropertyValue(kBorderRightColorProperty));

  style->set_border_right_color(KeywordValue::GetInitial());
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
     BorderRightColorComputedInitialValueIsSameAsColor) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

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
     BorderBottomColorSettersAndGettersAreConsistent) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

  EXPECT_EQ(style->border_bottom_color(),
            style->GetPropertyValue(kBorderBottomColorProperty));

  style->set_border_bottom_color(KeywordValue::GetInitial());
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
     BorderBottomColorComputedInitialValueIsSameAsColor) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

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

TEST(CSSComputedStyleDataTest, BorderLeftColorSettersAndGettersAreConsistent) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

  EXPECT_EQ(style->border_left_color(),
            style->GetPropertyValue(kBorderLeftColorProperty));

  style->set_border_left_color(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->border_left_color());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kBorderLeftColorProperty));

  style->SetPropertyValue(kBorderLeftColorProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->border_left_color());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kBorderLeftColorProperty));
}

TEST(CSSComputedStyleDataTest,
     BorderLeftColorComputedInitialValueIsSameAsColor) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

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

TEST(CSSComputedStyleDataTest, BorderTopStyleSettersAndGettersAreConsistent) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kBorderTopStyleProperty),
            style->border_top_style());
  EXPECT_EQ(style->border_top_style(),
            style->GetPropertyValue(kBorderTopStyleProperty));

  style->set_border_top_style(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->border_top_style());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kBorderTopStyleProperty));

  style->SetPropertyValue(kBorderTopStyleProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->border_top_style());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kBorderTopStyleProperty));
}

TEST(CSSComputedStyleDataTest, BorderRightStyleSettersAndGettersAreConsistent) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kBorderRightStyleProperty),
            style->border_right_style());
  EXPECT_EQ(style->border_right_style(),
            style->GetPropertyValue(kBorderRightStyleProperty));

  style->set_border_right_style(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->border_right_style());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kBorderRightStyleProperty));

  style->SetPropertyValue(kBorderRightStyleProperty,
                          KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->border_right_style());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kBorderRightStyleProperty));
}

TEST(CSSComputedStyleDataTest,
     BorderBottomStyleSettersAndGettersAreConsistent) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kBorderBottomStyleProperty),
            style->border_bottom_style());
  EXPECT_EQ(style->border_bottom_style(),
            style->GetPropertyValue(kBorderBottomStyleProperty));

  style->set_border_bottom_style(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->border_bottom_style());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kBorderBottomStyleProperty));

  style->SetPropertyValue(kBorderBottomStyleProperty,
                          KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->border_bottom_style());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kBorderBottomStyleProperty));
}

TEST(CSSComputedStyleDataTest, BorderLeftStyleSettersAndGettersAreConsistent) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kBorderLeftStyleProperty),
            style->border_left_style());
  EXPECT_EQ(style->border_left_style(),
            style->GetPropertyValue(kBorderLeftStyleProperty));

  style->set_border_left_style(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->border_left_style());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kBorderLeftStyleProperty));

  style->SetPropertyValue(kBorderLeftStyleProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->border_left_style());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kBorderLeftStyleProperty));
}

TEST(CSSComputedStyleDataTest, BorderTopWidthSettersAndGettersAreConsistent) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

  style->set_border_top_style(KeywordValue::GetSolid());
  EXPECT_EQ(GetPropertyInitialValue(kBorderTopWidthProperty),
            style->border_top_width());
  EXPECT_EQ(style->border_top_width(),
            style->GetPropertyValue(kBorderTopWidthProperty));

  style->set_border_top_width(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->border_top_width());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kBorderTopWidthProperty));

  style->SetPropertyValue(kBorderTopWidthProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->border_top_width());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kBorderTopWidthProperty));
}

TEST(CSSComputedStyleDataTest, BorderTopWidthIsZeroWhenStyleIsNoneOrHidden) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

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

TEST(CSSComputedStyleDataTest, BorderRightWidthSettersAndGettersAreConsistent) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

  style->set_border_right_style(KeywordValue::GetSolid());
  EXPECT_EQ(GetPropertyInitialValue(kBorderRightWidthProperty),
            style->border_right_width());
  EXPECT_EQ(style->border_right_width(),
            style->GetPropertyValue(kBorderRightWidthProperty));

  style->set_border_right_width(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->border_right_width());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kBorderRightWidthProperty));

  style->SetPropertyValue(kBorderRightWidthProperty,
                          KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->border_right_width());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kBorderRightWidthProperty));
}

TEST(CSSComputedStyleDataTest, BorderRightWidthIsZeroWhenStyleIsNoneOrHidden) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

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

TEST(CSSComputedStyleDataTest,
     BorderBottomWidthSettersAndGettersAreConsistent) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

  EXPECT_EQ(style->border_bottom_width(),
            style->GetPropertyValue(kBorderBottomWidthProperty));

  style->set_border_bottom_style(KeywordValue::GetSolid());
  EXPECT_EQ(GetPropertyInitialValue(kBorderBottomWidthProperty),
            style->border_bottom_width());
  style->set_border_bottom_width(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->border_bottom_width());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kBorderBottomWidthProperty));

  style->SetPropertyValue(kBorderBottomWidthProperty,
                          KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->border_bottom_width());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kBorderBottomWidthProperty));
}

TEST(CSSComputedStyleDataTest, BorderBottomWidthIsZeroWhenStyleIsNoneOrHidden) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

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

TEST(CSSComputedStyleDataTest, BorderLeftWidthSettersAndGettersAreConsistent) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

  style->set_border_left_style(KeywordValue::GetSolid());
  EXPECT_EQ(GetPropertyInitialValue(kBorderLeftWidthProperty),
            style->border_left_width());
  EXPECT_EQ(style->border_left_width(),
            style->GetPropertyValue(kBorderLeftWidthProperty));

  style->set_border_left_width(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->border_left_width());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kBorderLeftWidthProperty));

  style->SetPropertyValue(kBorderLeftWidthProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->border_left_width());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kBorderLeftWidthProperty));
}

TEST(CSSComputedStyleDataTest, BorderLeftWidthIsZeroWhenStyleIsNoneOrHidden) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

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

TEST(CSSComputedStyleDataTest, BorderRadiusSettersAndGettersAreConsistent) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kBorderTopLeftRadiusProperty),
            style->border_top_left_radius());
  EXPECT_EQ(style->border_top_left_radius(),
            style->GetPropertyValue(kBorderTopLeftRadiusProperty));

  style->set_border_top_left_radius(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->border_top_left_radius());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kBorderTopLeftRadiusProperty));

  style->SetPropertyValue(kBorderTopLeftRadiusProperty,
                          KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->border_top_left_radius());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kBorderTopLeftRadiusProperty));

  EXPECT_EQ(GetPropertyInitialValue(kBorderTopRightRadiusProperty),
            style->border_top_right_radius());
  EXPECT_EQ(style->border_top_right_radius(),
            style->GetPropertyValue(kBorderTopRightRadiusProperty));

  style->set_border_top_right_radius(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->border_top_right_radius());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kBorderTopRightRadiusProperty));

  style->SetPropertyValue(kBorderTopRightRadiusProperty,
                          KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->border_top_right_radius());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kBorderTopRightRadiusProperty));

  EXPECT_EQ(GetPropertyInitialValue(kBorderBottomRightRadiusProperty),
            style->border_bottom_right_radius());
  EXPECT_EQ(style->border_bottom_right_radius(),
            style->GetPropertyValue(kBorderBottomRightRadiusProperty));

  style->set_border_bottom_right_radius(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->border_bottom_right_radius());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kBorderBottomRightRadiusProperty));

  style->SetPropertyValue(kBorderBottomRightRadiusProperty,
                          KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->border_bottom_right_radius());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kBorderBottomRightRadiusProperty));

  EXPECT_EQ(GetPropertyInitialValue(kBorderBottomLeftRadiusProperty),
            style->border_bottom_left_radius());
  EXPECT_EQ(style->border_bottom_left_radius(),
            style->GetPropertyValue(kBorderBottomLeftRadiusProperty));

  style->set_border_bottom_left_radius(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->border_bottom_left_radius());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kBorderBottomLeftRadiusProperty));

  style->SetPropertyValue(kBorderBottomLeftRadiusProperty,
                          KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->border_bottom_left_radius());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kBorderBottomLeftRadiusProperty));
}

TEST(CSSComputedStyleDataTest, BottomSettersAndGettersAreConsistent) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kBottomProperty), style->bottom());
  EXPECT_EQ(style->bottom(), style->GetPropertyValue(kBottomProperty));

  style->set_bottom(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->bottom());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kBottomProperty));

  style->SetPropertyValue(kBottomProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->bottom());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kBottomProperty));
}

TEST(CSSComputedStyleDataTest, BoxShadowSettersAndGettersAreConsistent) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kBoxShadowProperty), style->box_shadow());
  EXPECT_EQ(style->box_shadow(), style->GetPropertyValue(kBoxShadowProperty));

  style->set_box_shadow(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->box_shadow());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kBoxShadowProperty));

  style->SetPropertyValue(kBoxShadowProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->box_shadow());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kBoxShadowProperty));
}

TEST(CSSComputedStyleDataTest, ColorSettersAndGettersAreConsistent) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

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
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

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
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

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

TEST(CSSComputedStyleDataTest, FilterSettersAndGettersAreConsistent) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kFilterProperty), style->filter());
  EXPECT_EQ(style->filter(), style->GetPropertyValue(kFilterProperty));

  style->set_filter(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->filter());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kFilterProperty));

  style->SetPropertyValue(kFilterProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->filter());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kFilterProperty));
}

TEST(CSSComputedStyleDataTest, FlexBasisSettersAndGettersAreConsistent) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kFlexBasisProperty), style->flex_basis());
  EXPECT_EQ(style->flex_basis(), style->GetPropertyValue(kFlexBasisProperty));

  style->set_flex_basis(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->flex_basis());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kFlexBasisProperty));

  style->SetPropertyValue(kFlexBasisProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->flex_basis());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kFlexBasisProperty));
}

TEST(CSSComputedStyleDataTest, FlexDirectionSettersAndGettersAreConsistent) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kFlexDirectionProperty),
            style->flex_direction());
  EXPECT_EQ(style->flex_direction(),
            style->GetPropertyValue(kFlexDirectionProperty));

  style->set_flex_direction(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->flex_direction());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kFlexDirectionProperty));

  style->SetPropertyValue(kFlexDirectionProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->flex_direction());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kFlexDirectionProperty));
}

TEST(CSSComputedStyleDataTest, FlexGrowSettersAndGettersAreConsistent) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kFlexGrowProperty), style->flex_grow());
  EXPECT_EQ(style->flex_grow(), style->GetPropertyValue(kFlexGrowProperty));

  style->set_flex_grow(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->flex_grow());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kFlexGrowProperty));

  style->SetPropertyValue(kFlexGrowProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->flex_grow());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kFlexGrowProperty));
}

TEST(CSSComputedStyleDataTest, FlexShrinkSettersAndGettersAreConsistent) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kFlexShrinkProperty), style->flex_shrink());
  EXPECT_EQ(style->flex_shrink(), style->GetPropertyValue(kFlexShrinkProperty));

  style->set_flex_shrink(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->flex_shrink());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kFlexShrinkProperty));

  style->SetPropertyValue(kFlexShrinkProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->flex_shrink());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kFlexShrinkProperty));
}

TEST(CSSComputedStyleDataTest, FlexWrapSettersAndGettersAreConsistent) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kFlexWrapProperty), style->flex_wrap());
  EXPECT_EQ(style->flex_wrap(), style->GetPropertyValue(kFlexWrapProperty));

  style->set_flex_wrap(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->flex_wrap());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kFlexWrapProperty));

  style->SetPropertyValue(kFlexWrapProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->flex_wrap());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kFlexWrapProperty));
}

TEST(CSSComputedStyleDataTest, FontFamilySettersAndGettersAreConsistent) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

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
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

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
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

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
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

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
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

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

TEST(CSSComputedStyleDataTest, JustifyContentSettersAndGettersAreConsistent) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kJustifyContentProperty),
            style->justify_content());
  EXPECT_EQ(style->justify_content(),
            style->GetPropertyValue(kJustifyContentProperty));

  style->set_justify_content(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->justify_content());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kJustifyContentProperty));

  style->SetPropertyValue(kJustifyContentProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->justify_content());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kJustifyContentProperty));
}

TEST(CSSComputedStyleDataTest, LeftSettersAndGettersAreConsistent) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kLeftProperty), style->left());
  EXPECT_EQ(style->left(), style->GetPropertyValue(kLeftProperty));

  style->set_left(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->left());
  EXPECT_EQ(KeywordValue::GetInitial(), style->GetPropertyValue(kLeftProperty));

  style->SetPropertyValue(kLeftProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->left());
  EXPECT_EQ(KeywordValue::GetInherit(), style->GetPropertyValue(kLeftProperty));
}

TEST(CSSComputedStyleDataTest, LineHeightSettersAndGettersAreConsistent) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

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
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

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
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

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
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

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
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

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
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

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
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

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
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

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
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

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
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

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

TEST(CSSComputedStyleDataTest, OrderSettersAndGettersAreConsistent) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kOrderProperty), style->order());
  EXPECT_EQ(style->order(), style->GetPropertyValue(kOrderProperty));

  style->set_order(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->order());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kOrderProperty));

  style->SetPropertyValue(kOrderProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->order());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kOrderProperty));
}

TEST(CSSComputedStyleDataTest, OutlineColorSettersAndGettersAreConsistent) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

  EXPECT_EQ(style->outline_color(),
            style->GetPropertyValue(kOutlineColorProperty));

  style->set_outline_color(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->outline_color());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kOutlineColorProperty));

  style->SetPropertyValue(kOutlineColorProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->outline_color());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kOutlineColorProperty));
}

TEST(CSSComputedStyleDataTest, OutlineColorComputedInitialValueIsSameAsColor) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kOutlineColorProperty),
            KeywordValue::GetCurrentColor());
  EXPECT_EQ(style->outline_color(),
            style->GetPropertyValue(kOutlineColorProperty));

  style->set_color(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->outline_color());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kOutlineColorProperty));

  style->SetPropertyValue(kOutlineColorProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->outline_color());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kOutlineColorProperty));
}

TEST(CSSComputedStyleDataTest, OutlineStyleSettersAndGettersAreConsistent) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kOutlineStyleProperty),
            style->outline_style());
  EXPECT_EQ(style->outline_style(),
            style->GetPropertyValue(kOutlineStyleProperty));

  style->set_outline_style(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->outline_style());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kOutlineStyleProperty));

  style->SetPropertyValue(kOutlineStyleProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->outline_style());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kOutlineStyleProperty));
}

TEST(CSSComputedStyleDataTest, OutlineWidthSettersAndGettersAreConsistent) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

  style->set_outline_style(KeywordValue::GetSolid());
  EXPECT_EQ(GetPropertyInitialValue(kOutlineWidthProperty),
            style->outline_width());
  EXPECT_EQ(style->outline_width(),
            style->GetPropertyValue(kOutlineWidthProperty));

  style->set_outline_width(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->outline_width());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kOutlineWidthProperty));

  style->SetPropertyValue(kOutlineWidthProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->outline_width());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kOutlineWidthProperty));
}

TEST(CSSComputedStyleDataTest, OutlineWidthIsZeroWhenStyleIsNoneOrHidden) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

  EXPECT_EQ(style->outline_width(),
            style->GetPropertyValue(kOutlineWidthProperty));

  style->set_outline_style(KeywordValue::GetSolid());
  EXPECT_EQ(GetPropertyInitialValue(kOutlineWidthProperty),
            style->outline_width());

  style->set_outline_style(KeywordValue::GetNone());
  LengthValue* length_value =
      dynamic_cast<LengthValue*>(style->outline_width().get());
  ASSERT_TRUE(length_value);
  EXPECT_EQ(0.0f, length_value->value());
  EXPECT_EQ(kPixelsUnit, length_value->unit());
  EXPECT_NE(GetPropertyInitialValue(kOutlineWidthProperty),
            style->outline_width());

  style->set_outline_style(KeywordValue::GetSolid());
  EXPECT_EQ(GetPropertyInitialValue(kOutlineWidthProperty),
            style->outline_width());

  style->set_outline_style(KeywordValue::GetHidden());
  length_value = dynamic_cast<LengthValue*>(style->outline_width().get());
  ASSERT_TRUE(length_value);
  EXPECT_EQ(0.0f, length_value->value());
  EXPECT_EQ(kPixelsUnit, length_value->unit());
  EXPECT_NE(GetPropertyInitialValue(kOutlineWidthProperty),
            style->outline_width());

  style->SetPropertyValue(kOutlineWidthProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->outline_width());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kOutlineWidthProperty));
}

TEST(CSSComputedStyleDataTest, OverflowSettersAndGettersAreConsistent) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

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
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

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
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

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
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

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
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

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
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

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

TEST(CSSComputedStyleDataTest, PointerEventsSettersAndGettersAreConsistent) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kPointerEventsProperty),
            style->pointer_events());
  EXPECT_EQ(style->pointer_events(),
            style->GetPropertyValue(kPointerEventsProperty));

  style->set_pointer_events(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->pointer_events());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kPointerEventsProperty));

  style->SetPropertyValue(kPointerEventsProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->pointer_events());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kPointerEventsProperty));
}

TEST(CSSComputedStyleDataTest, PositionSettersAndGettersAreConsistent) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

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

TEST(CSSComputedStyleDataTest, RightSettersAndGettersAreConsistent) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kRightProperty), style->right());
  EXPECT_EQ(style->right(), style->GetPropertyValue(kRightProperty));

  style->set_right(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->right());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kRightProperty));

  style->SetPropertyValue(kRightProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->right());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kRightProperty));
}

TEST(CSSComputedStyleDataTest, TextAlignSettersAndGettersAreConsistent) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

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
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

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
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

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
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

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
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

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

TEST(CSSComputedStyleDataTest, TextShadowSettersAndGettersAreConsistent) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kTextShadowProperty), style->text_shadow());
  EXPECT_EQ(style->text_shadow(), style->GetPropertyValue(kTextShadowProperty));

  style->set_text_shadow(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->text_shadow());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kTextShadowProperty));

  style->SetPropertyValue(kTextShadowProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->text_shadow());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kTextShadowProperty));
}

TEST(CSSComputedStyleDataTest, TextTransformSettersAndGettersAreConsistent) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

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

TEST(CSSComputedStyleDataTest, TopSettersAndGettersAreConsistent) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kTopProperty), style->top());
  EXPECT_EQ(style->top(), style->GetPropertyValue(kTopProperty));

  style->set_top(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->top());
  EXPECT_EQ(KeywordValue::GetInitial(), style->GetPropertyValue(kTopProperty));

  style->SetPropertyValue(kTopProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->top());
  EXPECT_EQ(KeywordValue::GetInherit(), style->GetPropertyValue(kTopProperty));
}

TEST(CSSComputedStyleDataTest, TransformSettersAndGettersAreConsistent) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

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

TEST(CSSComputedStyleDataTest, TransformOriginSettersAndGettersAreConsistent) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

  EXPECT_EQ(GetPropertyInitialValue(kTransformOriginProperty),
            style->transform_origin());
  EXPECT_EQ(style->transform_origin(),
            style->GetPropertyValue(kTransformOriginProperty));

  style->set_transform_origin(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->transform_origin());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kTransformOriginProperty));

  style->SetPropertyValue(kTransformOriginProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->transform_origin());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kTransformOriginProperty));
}

TEST(CSSComputedStyleDataTest, TransitionDelaySettersAndGettersAreConsistent) {
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

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
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

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
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

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
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

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
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

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
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

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
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

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
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

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
  scoped_refptr<MutableCSSComputedStyleData> style =
      new MutableCSSComputedStyleData();

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

TEST(CSSComputedStyleDataTest, TwoComputedStyleDataWithSamePropertiesAreEqual) {
  scoped_refptr<MutableCSSComputedStyleData> style1 =
      new MutableCSSComputedStyleData();
  style1->SetPropertyValue(kBorderLeftWidthProperty,
                           KeywordValue::GetInherit());
  scoped_refptr<MutableCSSComputedStyleData> style2 =
      new MutableCSSComputedStyleData();
  style2->SetPropertyValue(kBorderLeftWidthProperty,
                           KeywordValue::GetInherit());

  ASSERT_TRUE(style1->DoDeclaredPropertiesMatch(style2));
  ASSERT_TRUE(style2->DoDeclaredPropertiesMatch(style1));
}

TEST(CSSComputedStyleDataTest,
     TwoComputedStyleDataWithDifferentPropertiesAreUnequal) {
  scoped_refptr<MutableCSSComputedStyleData> style1 =
      new MutableCSSComputedStyleData();
  style1->SetPropertyValue(kBorderLeftWidthProperty,
                           KeywordValue::GetInherit());
  scoped_refptr<MutableCSSComputedStyleData> style2 =
      new MutableCSSComputedStyleData();
  style2->SetPropertyValue(kBorderRightWidthProperty,
                           KeywordValue::GetInherit());

  ASSERT_FALSE(style1->DoDeclaredPropertiesMatch(style2));
  ASSERT_FALSE(style2->DoDeclaredPropertiesMatch(style1));
}

TEST(CSSComputedStyleDataTest,
     ComputedStyleDataIsUnequalToComputedStyleDataWithPropertySuperset) {
  scoped_refptr<MutableCSSComputedStyleData> style1 =
      new MutableCSSComputedStyleData();
  style1->SetPropertyValue(kBorderLeftWidthProperty,
                           KeywordValue::GetInherit());
  scoped_refptr<MutableCSSComputedStyleData> style2 =
      new MutableCSSComputedStyleData();
  style2->SetPropertyValue(kBorderLeftWidthProperty,
                           KeywordValue::GetInherit());
  style2->SetPropertyValue(kBorderRightWidthProperty,
                           KeywordValue::GetInherit());

  ASSERT_FALSE(style1->DoDeclaredPropertiesMatch(style2));
  ASSERT_FALSE(style2->DoDeclaredPropertiesMatch(style1));
}

TEST(CSSComputedStyleDataTest,
     ComputedStyleDataIsUnequalToComputedStyleDataWithPropertySubset) {
  scoped_refptr<MutableCSSComputedStyleData> style1 =
      new MutableCSSComputedStyleData();
  style1->SetPropertyValue(kBorderLeftWidthProperty,
                           KeywordValue::GetInherit());
  style1->SetPropertyValue(kBorderRightWidthProperty,
                           KeywordValue::GetInherit());
  scoped_refptr<MutableCSSComputedStyleData> style2 =
      new MutableCSSComputedStyleData();
  style2->SetPropertyValue(kBorderLeftWidthProperty,
                           KeywordValue::GetInherit());

  ASSERT_FALSE(style1->DoDeclaredPropertiesMatch(style2));
  ASSERT_FALSE(style2->DoDeclaredPropertiesMatch(style1));
}

TEST(CSSComputedStyleDataTest,
     DoDeclaredPropertiesMatchWorksWithUnequalNumberOfDeclaredProperties) {
  scoped_refptr<MutableCSSComputedStyleData> style1 =
      new MutableCSSComputedStyleData();
  style1->set_font_size(new LengthValue(50, kPixelsUnit));

  scoped_refptr<MutableCSSComputedStyleData> style2 =
      new MutableCSSComputedStyleData();

  ASSERT_FALSE(style1->DoDeclaredPropertiesMatch(style2));
  ASSERT_FALSE(style2->DoDeclaredPropertiesMatch(style1));
}

TEST(CSSComputedStyleDataTest,
     DoDeclaredPropertiesMatchWorksWithSingleUnequalProperty) {
  scoped_refptr<MutableCSSComputedStyleData> style1 =
      new MutableCSSComputedStyleData();
  style1->set_font_size(new LengthValue(50, kPixelsUnit));

  scoped_refptr<MutableCSSComputedStyleData> style2 =
      new MutableCSSComputedStyleData();
  style2->set_font_size(new LengthValue(30, kPixelsUnit));

  ASSERT_FALSE(style1->DoDeclaredPropertiesMatch(style2));
  ASSERT_FALSE(style2->DoDeclaredPropertiesMatch(style1));
}

TEST(CSSComputedStyleDataTest,
     DoDeclaredPropertiesMatchWorksWithSingleEqualProperty) {
  scoped_refptr<MutableCSSComputedStyleData> style1 =
      new MutableCSSComputedStyleData();
  style1->set_font_size(new LengthValue(50, kPixelsUnit));

  scoped_refptr<MutableCSSComputedStyleData> style2 =
      new MutableCSSComputedStyleData();
  style2->set_font_size(new LengthValue(50, kPixelsUnit));

  ASSERT_TRUE(style1->DoDeclaredPropertiesMatch(style2));
  ASSERT_TRUE(style2->DoDeclaredPropertiesMatch(style1));
}

TEST(CSSComputedStyleDataTest,
     DoDeclaredPropertiesMatchWorksWithMultipleUnequalProperties) {
  scoped_refptr<MutableCSSComputedStyleData> style1 =
      new MutableCSSComputedStyleData();
  style1->set_position(KeywordValue::GetAbsolute());
  style1->set_font_size(new LengthValue(50, kPixelsUnit));

  scoped_refptr<MutableCSSComputedStyleData> style2 =
      new MutableCSSComputedStyleData();
  style2->set_position(KeywordValue::GetAbsolute());
  style2->set_font_size(new LengthValue(30, kPixelsUnit));

  ASSERT_FALSE(style1->DoDeclaredPropertiesMatch(style2));
  ASSERT_FALSE(style2->DoDeclaredPropertiesMatch(style1));
}

TEST(CSSComputedStyleDataTest,
     DoDeclaredPropertiesMatchWorksWithMultipleEqualProperty) {
  scoped_refptr<MutableCSSComputedStyleData> style1 =
      new MutableCSSComputedStyleData();
  style1->set_position(KeywordValue::GetAbsolute());
  style1->set_font_size(new LengthValue(50, kPixelsUnit));

  scoped_refptr<MutableCSSComputedStyleData> style2 =
      new MutableCSSComputedStyleData();
  style2->set_position(KeywordValue::GetAbsolute());
  style2->set_font_size(new LengthValue(50, kPixelsUnit));

  ASSERT_TRUE(style1->DoDeclaredPropertiesMatch(style2));
  ASSERT_TRUE(style2->DoDeclaredPropertiesMatch(style1));
}

TEST(CSSComputedStyleDataTest, AssignFromEmptyStyle) {
  scoped_refptr<CSSComputedStyleData> style_in = new CSSComputedStyleData();
  scoped_refptr<CSSComputedStyleData> style_copy = new CSSComputedStyleData();
  style_copy->AssignFrom(*style_in);
  style_in->DoDeclaredPropertiesMatch(style_copy);
  style_copy->DoDeclaredPropertiesMatch(style_in);
}

TEST(CSSComputedStyleDataTest, AssignFromCopiesAllLonghandProperties) {
  scoped_refptr<MutableCSSComputedStyleData> style_in =
      new MutableCSSComputedStyleData();
  scoped_refptr<CSSComputedStyleData> style_copy = new CSSComputedStyleData();

  for (int i = 0; i <= cssom::kMaxLonghandPropertyKey; ++i) {
    cssom::PropertyKey key = static_cast<cssom::PropertyKey>(i);
    style_in->SetPropertyValue(key, new cssom::IntegerValue(i));
  }

  style_copy->AssignFrom(*style_in);
  style_in->DoDeclaredPropertiesMatch(style_copy);
  style_copy->DoDeclaredPropertiesMatch(style_in);
}

TEST(CSSComputedStyleDataTest, AssignFromCopiesBeforeBlockificationStyle) {
  scoped_refptr<MutableCSSComputedStyleData> style_in =
      new MutableCSSComputedStyleData();
  scoped_refptr<CSSComputedStyleData> style_copy = new CSSComputedStyleData();

  style_in->set_is_inline_before_blockification(true);
  style_copy->AssignFrom(*style_in);
  EXPECT_TRUE(style_copy->is_inline_before_blockification());

  style_in->set_is_inline_before_blockification(false);
  style_copy->AssignFrom(*style_in);
  EXPECT_FALSE(style_copy->is_inline_before_blockification());
}

}  // namespace cssom
}  // namespace cobalt
