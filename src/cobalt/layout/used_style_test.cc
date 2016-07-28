/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/layout/used_style.h"

#include "cobalt/cssom/calc_value.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/percentage_value.h"
#include "cobalt/math/size_f.h"
#include "cobalt/math/size.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace layout {

// TODO: Add more tests for other style provider.

TEST(UsedStyleTest, UsedBackgroundPositionProviderWithPercentage) {
  scoped_ptr<cssom::PropertyListValue::Builder> property_value_builder(
      new cssom::PropertyListValue::Builder());
  property_value_builder->push_back(
      new cssom::CalcValue(new cssom::LengthValue(0.0f, cssom::kPixelsUnit),
                           new cssom::PercentageValue(0.6f)));
  property_value_builder->push_back(
      new cssom::CalcValue(new cssom::LengthValue(0.0f, cssom::kPixelsUnit),
                           new cssom::PercentageValue(0.8f)));
  scoped_refptr<cssom::PropertyListValue> property_value(
      new cssom::PropertyListValue(property_value_builder.Pass()));

  const math::SizeF frame_size(100, 200);
  const math::Size image_size(50, 50);
  UsedBackgroundPositionProvider used_background_position_provider(frame_size,
                                                                   image_size);
  property_value->Accept(&used_background_position_provider);
  EXPECT_FLOAT_EQ(used_background_position_provider.translate_x(), 30.0f);
  EXPECT_FLOAT_EQ(used_background_position_provider.translate_y(), 120.0f);
  EXPECT_FLOAT_EQ(
      used_background_position_provider.translate_x_relative_to_frame(), 0.3f);
  EXPECT_FLOAT_EQ(
      used_background_position_provider.translate_y_relative_to_frame(), 0.6f);
}

TEST(UsedStyleTest, UsedBackgroundPositionProviderWithLengthList) {
  scoped_ptr<cssom::PropertyListValue::Builder> property_value_builder(
      new cssom::PropertyListValue::Builder());
  property_value_builder->push_back(
      new cssom::CalcValue(new cssom::LengthValue(60, cssom::kPixelsUnit),
                           new cssom::PercentageValue(0.0f)));
  property_value_builder->push_back(
      new cssom::CalcValue(new cssom::LengthValue(80, cssom::kPixelsUnit),
                           new cssom::PercentageValue(0.0f)));
  scoped_refptr<cssom::PropertyListValue> property_value(
      new cssom::PropertyListValue(property_value_builder.Pass()));

  const math::SizeF frame_size(100, 200);
  const math::Size image_size(50, 50);
  UsedBackgroundPositionProvider used_background_position_provider(frame_size,
                                                                   image_size);
  property_value->Accept(&used_background_position_provider);
  EXPECT_FLOAT_EQ(used_background_position_provider.translate_x(), 60.0f);
  EXPECT_FLOAT_EQ(used_background_position_provider.translate_y(), 80.0f);
  EXPECT_FLOAT_EQ(
      used_background_position_provider.translate_x_relative_to_frame(), 0.6f);
  EXPECT_FLOAT_EQ(
      used_background_position_provider.translate_y_relative_to_frame(), 0.4f);
}

TEST(UsedStyleTest, UsedBackgroundPositionProviderWithLengthAndPercentageList) {
  scoped_ptr<cssom::PropertyListValue::Builder> property_value_builder(
      new cssom::PropertyListValue::Builder());

  property_value_builder->push_back(
      new cssom::CalcValue(new cssom::LengthValue(60.0f, cssom::kPixelsUnit),
                           new cssom::PercentageValue(0.0f)));
  property_value_builder->push_back(
      new cssom::CalcValue(new cssom::LengthValue(-80.0f, cssom::kPixelsUnit),
                           new cssom::PercentageValue(1.0f)));
  scoped_refptr<cssom::PropertyListValue> property_value(
      new cssom::PropertyListValue(property_value_builder.Pass()));

  const math::SizeF frame_size(100, 200);
  const math::Size image_size(50, 50);
  UsedBackgroundPositionProvider used_background_position_provider(frame_size,
                                                                   image_size);
  property_value->Accept(&used_background_position_provider);
  EXPECT_FLOAT_EQ(used_background_position_provider.translate_x(), 60.0f);
  EXPECT_FLOAT_EQ(used_background_position_provider.translate_y(), 70.0f);
  EXPECT_FLOAT_EQ(
      used_background_position_provider.translate_x_relative_to_frame(), 0.6f);
  EXPECT_FLOAT_EQ(
      used_background_position_provider.translate_y_relative_to_frame(), 0.35f);
}

TEST(UsedStyleTest, UsedBackgroundSizeProviderContainWithWidthScale) {
  const math::SizeF frame_size(100, 200);
  const math::Size image_size(50, 50);
  UsedBackgroundSizeProvider used_background_size_provider(frame_size,
                                                           image_size);
  cssom::KeywordValue::GetContain()->Accept(&used_background_size_provider);
  EXPECT_FLOAT_EQ(used_background_size_provider.width(), 100.0f);
  EXPECT_FLOAT_EQ(used_background_size_provider.height(), 100.0f);
  EXPECT_FLOAT_EQ(used_background_size_provider.width_scale_relative_to_frame(),
                  1.0f);
  EXPECT_FLOAT_EQ(
      used_background_size_provider.height_scale_relative_to_frame(), 0.5f);
}

TEST(UsedStyleTest, UsedBackgroundSizeProviderContainWithHeightScale) {
  const math::SizeF frame_size(200, 100);
  const math::Size image_size(50, 50);
  UsedBackgroundSizeProvider used_background_size_provider(frame_size,
                                                           image_size);
  cssom::KeywordValue::GetContain()->Accept(&used_background_size_provider);
  EXPECT_FLOAT_EQ(used_background_size_provider.width(), 100.0f);
  EXPECT_FLOAT_EQ(used_background_size_provider.height(), 100.0f);
  EXPECT_FLOAT_EQ(used_background_size_provider.width_scale_relative_to_frame(),
                  0.5f);
  EXPECT_FLOAT_EQ(
      used_background_size_provider.height_scale_relative_to_frame(), 1.0f);
}

TEST(UsedStyleTest, UsedBackgroundSizeProviderCoverWithWidthScale) {
  const math::SizeF frame_size(200, 100);
  const math::Size image_size(50, 50);
  UsedBackgroundSizeProvider used_background_size_provider(frame_size,
                                                           image_size);
  cssom::KeywordValue::GetCover()->Accept(&used_background_size_provider);
  EXPECT_FLOAT_EQ(used_background_size_provider.width(), 200.0f);
  EXPECT_FLOAT_EQ(used_background_size_provider.height(), 200.0f);
  EXPECT_FLOAT_EQ(used_background_size_provider.width_scale_relative_to_frame(),
                  1.0f);
  EXPECT_FLOAT_EQ(
      used_background_size_provider.height_scale_relative_to_frame(), 2.0f);
}

TEST(UsedStyleTest, UsedBackgroundSizeProviderCoverWithHeightScale) {
  const math::SizeF frame_size(100, 200);
  const math::Size image_size(50, 50);
  UsedBackgroundSizeProvider used_background_size_provider(frame_size,
                                                           image_size);
  cssom::KeywordValue::GetCover()->Accept(&used_background_size_provider);
  EXPECT_FLOAT_EQ(used_background_size_provider.width(), 200.0f);
  EXPECT_FLOAT_EQ(used_background_size_provider.height(), 200.0f);
  EXPECT_FLOAT_EQ(used_background_size_provider.width_scale_relative_to_frame(),
                  2.0f);
  EXPECT_FLOAT_EQ(
      used_background_size_provider.height_scale_relative_to_frame(), 1.0f);
}

TEST(UsedStyleTest, UsedBackgroundSizeProviderWithDoubleAutoPropertyList) {
  scoped_ptr<cssom::PropertyListValue::Builder> property_value_builder(
      new cssom::PropertyListValue::Builder());
  property_value_builder->push_back(cssom::KeywordValue::GetAuto());
  property_value_builder->push_back(cssom::KeywordValue::GetAuto());
  scoped_refptr<cssom::PropertyListValue> property_value(
      new cssom::PropertyListValue(property_value_builder.Pass()));

  const math::SizeF frame_size(100, 200);
  const math::Size image_size(50, 50);
  UsedBackgroundSizeProvider used_background_size_provider(frame_size,
                                                           image_size);
  property_value->Accept(&used_background_size_provider);
  EXPECT_FLOAT_EQ(used_background_size_provider.width(), 50.0f);
  EXPECT_FLOAT_EQ(used_background_size_provider.height(), 50.0f);
  EXPECT_FLOAT_EQ(used_background_size_provider.width_scale_relative_to_frame(),
                  0.5f);
  EXPECT_FLOAT_EQ(
      used_background_size_provider.height_scale_relative_to_frame(), 0.25f);
}

TEST(UsedStyleTest,
     UsedBackgroundSizeProviderWithDoublePercentagePropertyList) {
  scoped_ptr<cssom::PropertyListValue::Builder> property_value_builder(
      new cssom::PropertyListValue::Builder());
  property_value_builder->push_back(new cssom::PercentageValue(0.5f));
  property_value_builder->push_back(new cssom::PercentageValue(0.8f));
  scoped_refptr<cssom::PropertyListValue> property_value(
      new cssom::PropertyListValue(property_value_builder.Pass()));

  const math::SizeF frame_size(200, 150);
  const math::Size image_size(50, 50);
  UsedBackgroundSizeProvider used_background_size_provider(frame_size,
                                                           image_size);
  property_value->Accept(&used_background_size_provider);
  EXPECT_FLOAT_EQ(used_background_size_provider.width(), 100.0f);
  EXPECT_FLOAT_EQ(used_background_size_provider.height(), 120.0f);
  EXPECT_FLOAT_EQ(used_background_size_provider.width_scale_relative_to_frame(),
                  0.5f);
  EXPECT_FLOAT_EQ(
      used_background_size_provider.height_scale_relative_to_frame(), 0.8f);
}

TEST(UsedStyleTest,
     UsedBackgroundSizeProviderWithAutoAndPercentagePropertyList) {
  scoped_ptr<cssom::PropertyListValue::Builder> property_value_builder(
      new cssom::PropertyListValue::Builder());
  property_value_builder->push_back(cssom::KeywordValue::GetAuto());
  property_value_builder->push_back(new cssom::PercentageValue(0.8f));
  scoped_refptr<cssom::PropertyListValue> property_value(
      new cssom::PropertyListValue(property_value_builder.Pass()));

  const math::SizeF frame_size(200, 150);
  const math::Size image_size(50, 50);
  UsedBackgroundSizeProvider used_background_size_provider(frame_size,
                                                           image_size);
  property_value->Accept(&used_background_size_provider);
  EXPECT_FLOAT_EQ(used_background_size_provider.width(), 120.0f);
  EXPECT_FLOAT_EQ(used_background_size_provider.height(), 120.0f);
  EXPECT_FLOAT_EQ(used_background_size_provider.width_scale_relative_to_frame(),
                  0.6f);
  EXPECT_FLOAT_EQ(
      used_background_size_provider.height_scale_relative_to_frame(), 0.8f);
}

TEST(UsedStyleTest,
     UsedBackgroundSizeProviderWithPercentageAndAutoPropertyList) {
  scoped_ptr<cssom::PropertyListValue::Builder> property_value_builder(
      new cssom::PropertyListValue::Builder());
  property_value_builder->push_back(new cssom::PercentageValue(0.5f));
  property_value_builder->push_back(cssom::KeywordValue::GetAuto());
  scoped_refptr<cssom::PropertyListValue> property_value(
      new cssom::PropertyListValue(property_value_builder.Pass()));

  const math::SizeF frame_size(200, 100);
  const math::Size image_size(50, 50);
  UsedBackgroundSizeProvider used_background_size_provider(frame_size,
                                                           image_size);
  property_value->Accept(&used_background_size_provider);
  EXPECT_FLOAT_EQ(used_background_size_provider.width(), 100.0f);
  EXPECT_FLOAT_EQ(used_background_size_provider.height(), 100.0f);
  EXPECT_FLOAT_EQ(used_background_size_provider.width_scale_relative_to_frame(),
                  0.5f);
  EXPECT_FLOAT_EQ(
      used_background_size_provider.height_scale_relative_to_frame(), 1.0f);
}

TEST(UsedStyleTest, UsedBackgroundSizeProviderWithLengthValuePropertyList) {
  scoped_ptr<cssom::PropertyListValue::Builder> property_value_builder(
      new cssom::PropertyListValue::Builder());
  property_value_builder->push_back(
      new cssom::LengthValue(30, cssom::kPixelsUnit));
  property_value_builder->push_back(
      new cssom::LengthValue(60, cssom::kPixelsUnit));
  scoped_refptr<cssom::PropertyListValue> property_value(
      new cssom::PropertyListValue(property_value_builder.Pass()));

  const math::SizeF frame_size(200, 100);
  const math::Size image_size(50, 50);
  UsedBackgroundSizeProvider used_background_size_provider(frame_size,
                                                           image_size);
  property_value->Accept(&used_background_size_provider);
  EXPECT_FLOAT_EQ(used_background_size_provider.width(), 30.0f);
  EXPECT_FLOAT_EQ(used_background_size_provider.height(), 60.0f);
  EXPECT_FLOAT_EQ(used_background_size_provider.width_scale_relative_to_frame(),
                  0.15f);
  EXPECT_FLOAT_EQ(
      used_background_size_provider.height_scale_relative_to_frame(), 0.6f);
}

TEST(UsedStyleTest, UsedBackgroundSizeProviderWithLengthAndAutoPropertyList) {
  scoped_ptr<cssom::PropertyListValue::Builder> property_value_builder(
      new cssom::PropertyListValue::Builder());
  property_value_builder->push_back(
      new cssom::LengthValue(30, cssom::kPixelsUnit));
  property_value_builder->push_back(cssom::KeywordValue::GetAuto());
  scoped_refptr<cssom::PropertyListValue> property_value(
      new cssom::PropertyListValue(property_value_builder.Pass()));

  const math::SizeF frame_size(200, 100);
  const math::Size image_size(50, 60);
  UsedBackgroundSizeProvider used_background_size_provider(frame_size,
                                                           image_size);
  property_value->Accept(&used_background_size_provider);
  EXPECT_FLOAT_EQ(used_background_size_provider.width(), 30.0f);
  EXPECT_FLOAT_EQ(used_background_size_provider.height(), 36.0f);
  EXPECT_FLOAT_EQ(used_background_size_provider.width_scale_relative_to_frame(),
                  0.15f);
  EXPECT_FLOAT_EQ(
      used_background_size_provider.height_scale_relative_to_frame(), 0.36f);
}

TEST(UsedStyleTest, UsedBackgroundRepeatProviderNoRepeatAndRepeat) {
  scoped_ptr<cssom::PropertyListValue::Builder> property_value_builder(
      new cssom::PropertyListValue::Builder());
  property_value_builder->push_back(cssom::KeywordValue::GetNoRepeat());
  property_value_builder->push_back(cssom::KeywordValue::GetRepeat());
  scoped_refptr<cssom::PropertyListValue> property_value(
      new cssom::PropertyListValue(property_value_builder.Pass()));

  UsedBackgroundRepeatProvider used_background_repeat_provider;
  property_value->Accept(&used_background_repeat_provider);
  EXPECT_FALSE(used_background_repeat_provider.repeat_x());
  EXPECT_TRUE(used_background_repeat_provider.repeat_y());
}

}  // namespace layout
}  // namespace cobalt
