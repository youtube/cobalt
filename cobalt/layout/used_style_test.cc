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

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/percentage_value.h"
#include "cobalt/cssom/property_names.h"
#include "cobalt/math/size_f.h"
#include "cobalt/math/size.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace layout {

// TODO(***REMOVED***): Add more tests for other style provider.

TEST(UsedStyleTest, UsedBackgroundSizeProviderContainWithWidthScale) {
  const math::SizeF frame_size(100, 200);
  const math::Size image_size(50, 50);
  UsedBackgroundSizeProvider used_background_size_provider(frame_size,
                                                           image_size);
  cssom::KeywordValue::GetContain()->Accept(&used_background_size_provider);
  EXPECT_FLOAT_EQ(used_background_size_provider.width_scale(), 1.0f);
  EXPECT_FLOAT_EQ(used_background_size_provider.height_scale(), 0.5f);
}

TEST(UsedStyleTest, UsedBackgroundSizeProviderContainWithHeightScale) {
  const math::SizeF frame_size(200, 100);
  const math::Size image_size(50, 50);
  UsedBackgroundSizeProvider used_background_size_provider(frame_size,
                                                           image_size);
  cssom::KeywordValue::GetContain()->Accept(&used_background_size_provider);
  EXPECT_FLOAT_EQ(used_background_size_provider.width_scale(), 0.5f);
  EXPECT_FLOAT_EQ(used_background_size_provider.height_scale(), 1.0f);
}

TEST(UsedStyleTest, UsedBackgroundSizeProviderCoverWithWidthScale) {
  const math::SizeF frame_size(200, 100);
  const math::Size image_size(50, 50);
  UsedBackgroundSizeProvider used_background_size_provider(frame_size,
                                                           image_size);
  cssom::KeywordValue::GetCover()->Accept(&used_background_size_provider);
  EXPECT_FLOAT_EQ(used_background_size_provider.width_scale(), 1.0f);
  EXPECT_FLOAT_EQ(used_background_size_provider.height_scale(), 2.0f);
}

TEST(UsedStyleTest, UsedBackgroundSizeProviderCoverWithHeightScale) {
  const math::SizeF frame_size(100, 200);
  const math::Size image_size(50, 50);
  UsedBackgroundSizeProvider used_background_size_provider(frame_size,
                                                           image_size);
  cssom::KeywordValue::GetCover()->Accept(&used_background_size_provider);
  EXPECT_FLOAT_EQ(used_background_size_provider.width_scale(), 2.0f);
  EXPECT_FLOAT_EQ(used_background_size_provider.height_scale(), 1.0f);
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
  EXPECT_FLOAT_EQ(used_background_size_provider.width_scale(), 0.5f);
  EXPECT_FLOAT_EQ(used_background_size_provider.height_scale(), 0.25f);
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
  EXPECT_FLOAT_EQ(used_background_size_provider.width_scale(), 0.5f);
  EXPECT_FLOAT_EQ(used_background_size_provider.height_scale(), 0.8f);
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
  EXPECT_FLOAT_EQ(used_background_size_provider.width_scale(), 0.6f);
  EXPECT_FLOAT_EQ(used_background_size_provider.height_scale(), 0.8f);
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
  EXPECT_FLOAT_EQ(used_background_size_provider.width_scale(), 0.5f);
  EXPECT_FLOAT_EQ(used_background_size_provider.height_scale(), 1.0f);
}
}  // namespace layout
}  // namespace cobalt
