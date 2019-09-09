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

#include <memory>

#include "cobalt/layout/used_style.h"

#include "cobalt/cssom/calc_value.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/percentage_value.h"
#include "cobalt/math/size.h"
#include "cobalt/math/size_f.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace layout {

// TODO: Add more tests for other style provider.

TEST(UsedStyleTest, UsedBackgroundPositionProviderWithPercentage) {
  std::unique_ptr<cssom::PropertyListValue::Builder> property_value_builder(
      new cssom::PropertyListValue::Builder());
  property_value_builder->push_back(
      new cssom::CalcValue(new cssom::LengthValue(0.0f, cssom::kPixelsUnit),
                           new cssom::PercentageValue(0.6f)));
  property_value_builder->push_back(
      new cssom::CalcValue(new cssom::LengthValue(0.0f, cssom::kPixelsUnit),
                           new cssom::PercentageValue(0.8f)));
  scoped_refptr<cssom::PropertyListValue> property_value(
      new cssom::PropertyListValue(std::move(property_value_builder)));

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
  std::unique_ptr<cssom::PropertyListValue::Builder> property_value_builder(
      new cssom::PropertyListValue::Builder());
  property_value_builder->push_back(
      new cssom::CalcValue(new cssom::LengthValue(60, cssom::kPixelsUnit),
                           new cssom::PercentageValue(0.0f)));
  property_value_builder->push_back(
      new cssom::CalcValue(new cssom::LengthValue(80, cssom::kPixelsUnit),
                           new cssom::PercentageValue(0.0f)));
  scoped_refptr<cssom::PropertyListValue> property_value(
      new cssom::PropertyListValue(std::move(property_value_builder)));

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
  std::unique_ptr<cssom::PropertyListValue::Builder> property_value_builder(
      new cssom::PropertyListValue::Builder());

  property_value_builder->push_back(
      new cssom::CalcValue(new cssom::LengthValue(60.0f, cssom::kPixelsUnit),
                           new cssom::PercentageValue(0.0f)));
  property_value_builder->push_back(
      new cssom::CalcValue(new cssom::LengthValue(-80.0f, cssom::kPixelsUnit),
                           new cssom::PercentageValue(1.0f)));
  scoped_refptr<cssom::PropertyListValue> property_value(
      new cssom::PropertyListValue(std::move(property_value_builder)));

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
  std::unique_ptr<cssom::PropertyListValue::Builder> property_value_builder(
      new cssom::PropertyListValue::Builder());
  property_value_builder->push_back(cssom::KeywordValue::GetAuto());
  property_value_builder->push_back(cssom::KeywordValue::GetAuto());
  scoped_refptr<cssom::PropertyListValue> property_value(
      new cssom::PropertyListValue(std::move(property_value_builder)));

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
  std::unique_ptr<cssom::PropertyListValue::Builder> property_value_builder(
      new cssom::PropertyListValue::Builder());
  property_value_builder->push_back(new cssom::PercentageValue(0.5f));
  property_value_builder->push_back(new cssom::PercentageValue(0.8f));
  scoped_refptr<cssom::PropertyListValue> property_value(
      new cssom::PropertyListValue(std::move(property_value_builder)));

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
  std::unique_ptr<cssom::PropertyListValue::Builder> property_value_builder(
      new cssom::PropertyListValue::Builder());
  property_value_builder->push_back(cssom::KeywordValue::GetAuto());
  property_value_builder->push_back(new cssom::PercentageValue(0.8f));
  scoped_refptr<cssom::PropertyListValue> property_value(
      new cssom::PropertyListValue(std::move(property_value_builder)));

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
  std::unique_ptr<cssom::PropertyListValue::Builder> property_value_builder(
      new cssom::PropertyListValue::Builder());
  property_value_builder->push_back(new cssom::PercentageValue(0.5f));
  property_value_builder->push_back(cssom::KeywordValue::GetAuto());
  scoped_refptr<cssom::PropertyListValue> property_value(
      new cssom::PropertyListValue(std::move(property_value_builder)));

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
  std::unique_ptr<cssom::PropertyListValue::Builder> property_value_builder(
      new cssom::PropertyListValue::Builder());
  property_value_builder->push_back(
      new cssom::LengthValue(30, cssom::kPixelsUnit));
  property_value_builder->push_back(
      new cssom::LengthValue(60, cssom::kPixelsUnit));
  scoped_refptr<cssom::PropertyListValue> property_value(
      new cssom::PropertyListValue(std::move(property_value_builder)));

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
  std::unique_ptr<cssom::PropertyListValue::Builder> property_value_builder(
      new cssom::PropertyListValue::Builder());
  property_value_builder->push_back(
      new cssom::LengthValue(30, cssom::kPixelsUnit));
  property_value_builder->push_back(cssom::KeywordValue::GetAuto());
  scoped_refptr<cssom::PropertyListValue> property_value(
      new cssom::PropertyListValue(std::move(property_value_builder)));

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
  std::unique_ptr<cssom::PropertyListValue::Builder> property_value_builder(
      new cssom::PropertyListValue::Builder());
  property_value_builder->push_back(cssom::KeywordValue::GetNoRepeat());
  property_value_builder->push_back(cssom::KeywordValue::GetRepeat());
  scoped_refptr<cssom::PropertyListValue> property_value(
      new cssom::PropertyListValue(std::move(property_value_builder)));

  UsedBackgroundRepeatProvider used_background_repeat_provider;
  property_value->Accept(&used_background_repeat_provider);
  EXPECT_FALSE(used_background_repeat_provider.repeat_x());
  EXPECT_TRUE(used_background_repeat_provider.repeat_y());
}

TEST(UsedStyleTest, UsedLeftAutoIsNotResolved) {
  scoped_refptr<cssom::MutableCSSComputedStyleData> computed_style(
      new cssom::MutableCSSComputedStyleData());
  computed_style->set_left(cssom::KeywordValue::GetAuto());
  SizeLayoutUnit containing_block_size(LayoutUnit(200.0f), LayoutUnit(400.0f));
  auto used_left = GetUsedLeftIfNotAuto(computed_style, containing_block_size);
  EXPECT_FALSE(used_left);
}

TEST(UsedStyleTest, UsedLeftPercentDependsOnContainingBlockWidth) {
  scoped_refptr<cssom::MutableCSSComputedStyleData> computed_style(
      new cssom::MutableCSSComputedStyleData());
  computed_style->set_left(new cssom::PercentageValue(0.25f));
  SizeLayoutUnit containing_block_size(LayoutUnit(200.0f), LayoutUnit(400.0f));
  auto used_left = GetUsedLeftIfNotAuto(computed_style, containing_block_size);
  EXPECT_TRUE(used_left);
  EXPECT_EQ(*used_left, LayoutUnit(50.0f));
}

TEST(UsedStyleTest, UsedLeftLengthValue) {
  scoped_refptr<cssom::MutableCSSComputedStyleData> computed_style(
      new cssom::MutableCSSComputedStyleData());
  computed_style->set_left(new cssom::LengthValue(25.0f, cssom::kPixelsUnit));
  SizeLayoutUnit containing_block_size(LayoutUnit(200.0f), LayoutUnit(400.0f));
  auto used_left = GetUsedLeftIfNotAuto(computed_style, containing_block_size);
  EXPECT_TRUE(used_left);
  EXPECT_EQ(*used_left, LayoutUnit(25.0f));
}

TEST(UsedStyleTest, UsedTopAutoIsNotResolved) {
  scoped_refptr<cssom::MutableCSSComputedStyleData> computed_style(
      new cssom::MutableCSSComputedStyleData());
  computed_style->set_top(cssom::KeywordValue::GetAuto());
  SizeLayoutUnit containing_block_size(LayoutUnit(200.0f), LayoutUnit(400.0f));
  auto used_top = GetUsedTopIfNotAuto(computed_style, containing_block_size);
  EXPECT_FALSE(used_top);
}

TEST(UsedStyleTest, UsedTopPercentDependsOnContainingBlockHeight) {
  scoped_refptr<cssom::MutableCSSComputedStyleData> computed_style(
      new cssom::MutableCSSComputedStyleData());
  computed_style->set_top(new cssom::PercentageValue(0.25f));
  SizeLayoutUnit containing_block_size(LayoutUnit(200.0f), LayoutUnit(400.0f));
  auto used_top = GetUsedTopIfNotAuto(computed_style, containing_block_size);
  EXPECT_TRUE(used_top);
  EXPECT_EQ(*used_top, LayoutUnit(100.0f));
}

TEST(UsedStyleTest, UsedTopLengthValue) {
  scoped_refptr<cssom::MutableCSSComputedStyleData> computed_style(
      new cssom::MutableCSSComputedStyleData());
  computed_style->set_top(new cssom::LengthValue(25.0f, cssom::kPixelsUnit));
  SizeLayoutUnit containing_block_size(LayoutUnit(200.0f), LayoutUnit(400.0f));
  auto used_top = GetUsedTopIfNotAuto(computed_style, containing_block_size);
  EXPECT_TRUE(used_top);
  EXPECT_EQ(*used_top, LayoutUnit(25.0f));
}

TEST(UsedStyleTest, UsedRightAutoIsNotResolved) {
  scoped_refptr<cssom::MutableCSSComputedStyleData> computed_style(
      new cssom::MutableCSSComputedStyleData());
  computed_style->set_right(cssom::KeywordValue::GetAuto());
  SizeLayoutUnit containing_block_size(LayoutUnit(200.0f), LayoutUnit(400.0f));
  auto used_right =
      GetUsedRightIfNotAuto(computed_style, containing_block_size);
  EXPECT_FALSE(used_right);
}

TEST(UsedStyleTest, UsedRightPercentDependsOnContainingBlockWidth) {
  scoped_refptr<cssom::MutableCSSComputedStyleData> computed_style(
      new cssom::MutableCSSComputedStyleData());
  computed_style->set_right(new cssom::PercentageValue(0.25f));
  SizeLayoutUnit containing_block_size(LayoutUnit(200.0f), LayoutUnit(400.0f));
  auto used_right =
      GetUsedRightIfNotAuto(computed_style, containing_block_size);
  EXPECT_TRUE(used_right);
  EXPECT_EQ(*used_right, LayoutUnit(50.0f));
}

TEST(UsedStyleTest, UsedRightLengthValue) {
  scoped_refptr<cssom::MutableCSSComputedStyleData> computed_style(
      new cssom::MutableCSSComputedStyleData());
  computed_style->set_right(new cssom::LengthValue(25.0f, cssom::kPixelsUnit));
  SizeLayoutUnit containing_block_size(LayoutUnit(200.0f), LayoutUnit(400.0f));
  auto used_right =
      GetUsedRightIfNotAuto(computed_style, containing_block_size);
  EXPECT_TRUE(used_right);
  EXPECT_EQ(*used_right, LayoutUnit(25.0f));
}

TEST(UsedStyleTest, UsedBottomAutoIsNotResolved) {
  scoped_refptr<cssom::MutableCSSComputedStyleData> computed_style(
      new cssom::MutableCSSComputedStyleData());
  computed_style->set_bottom(cssom::KeywordValue::GetAuto());
  SizeLayoutUnit containing_block_size(LayoutUnit(200.0f), LayoutUnit(400.0f));
  auto used_bottom =
      GetUsedBottomIfNotAuto(computed_style, containing_block_size);
  EXPECT_FALSE(used_bottom);
}

TEST(UsedStyleTest, UsedBottomPercentDependsOnContainingBlockHeight) {
  scoped_refptr<cssom::MutableCSSComputedStyleData> computed_style(
      new cssom::MutableCSSComputedStyleData());
  computed_style->set_bottom(new cssom::PercentageValue(0.25f));
  SizeLayoutUnit containing_block_size(LayoutUnit(200.0f), LayoutUnit(400.0f));
  auto used_bottom =
      GetUsedBottomIfNotAuto(computed_style, containing_block_size);
  EXPECT_TRUE(used_bottom);
  EXPECT_EQ(*used_bottom, LayoutUnit(100.0f));
}

TEST(UsedStyleTest, UsedBottomLengthValue) {
  scoped_refptr<cssom::MutableCSSComputedStyleData> computed_style(
      new cssom::MutableCSSComputedStyleData());
  computed_style->set_bottom(new cssom::LengthValue(25.0f, cssom::kPixelsUnit));
  SizeLayoutUnit containing_block_size(LayoutUnit(200.0f), LayoutUnit(400.0f));
  auto used_bottom =
      GetUsedBottomIfNotAuto(computed_style, containing_block_size);
  EXPECT_TRUE(used_bottom);
  EXPECT_EQ(*used_bottom, LayoutUnit(25.0f));
}

TEST(UsedStyleTest, UsedFlexBasisAutoDependsOnFlexContainer) {
  scoped_refptr<cssom::MutableCSSComputedStyleData> computed_style(
      new cssom::MutableCSSComputedStyleData());
  computed_style->set_flex_basis(cssom::KeywordValue::GetAuto());
  SizeLayoutUnit flex_container_size(LayoutUnit(200), LayoutUnit(80));
  bool depends_on_flex_container = false;
  auto used_flex_basis = GetUsedFlexBasisIfNotContent(
      computed_style, true, flex_container_size.width(),
      &depends_on_flex_container);
  EXPECT_TRUE(depends_on_flex_container);
  EXPECT_FALSE(used_flex_basis);

  depends_on_flex_container = false;
  used_flex_basis = GetUsedFlexBasisIfNotContent(computed_style, false,
                                                 flex_container_size.height(),
                                                 &depends_on_flex_container);
  EXPECT_TRUE(depends_on_flex_container);
  EXPECT_FALSE(used_flex_basis);
}

TEST(UsedStyleTest, UsedFlexBasisPercentDependsOnFlexContainerMainSize) {
  scoped_refptr<cssom::MutableCSSComputedStyleData> computed_style(
      new cssom::MutableCSSComputedStyleData());
  computed_style->set_flex_basis(new cssom::PercentageValue(0.25f));
  SizeLayoutUnit flex_container_size(LayoutUnit(200), LayoutUnit(80));
  bool depends_on_flex_container = false;
  auto used_flex_basis = GetUsedFlexBasisIfNotContent(
      computed_style, true, flex_container_size.width(),
      &depends_on_flex_container);
  EXPECT_TRUE(depends_on_flex_container);
  EXPECT_TRUE(used_flex_basis);
  EXPECT_EQ(*used_flex_basis, LayoutUnit(50));

  depends_on_flex_container = false;
  used_flex_basis = GetUsedFlexBasisIfNotContent(computed_style, false,
                                                 flex_container_size.height(),
                                                 &depends_on_flex_container);
  EXPECT_TRUE(depends_on_flex_container);
  EXPECT_TRUE(used_flex_basis);
  EXPECT_EQ(*used_flex_basis, LayoutUnit(20));
}

TEST(UsedStyleTest, UsedFlexBasisLengthValue) {
  scoped_refptr<cssom::MutableCSSComputedStyleData> computed_style(
      new cssom::MutableCSSComputedStyleData());
  computed_style->set_flex_basis(
      new cssom::LengthValue(25.0f, cssom::kPixelsUnit));
  SizeLayoutUnit flex_container_size(LayoutUnit(200), LayoutUnit(80));
  bool depends_on_flex_container = false;
  auto used_flex_basis = GetUsedFlexBasisIfNotContent(
      computed_style, true, flex_container_size.width(),
      &depends_on_flex_container);
  EXPECT_FALSE(depends_on_flex_container);
  EXPECT_TRUE(used_flex_basis);
  EXPECT_EQ(*used_flex_basis, LayoutUnit(25));

  depends_on_flex_container = false;
  used_flex_basis = GetUsedFlexBasisIfNotContent(computed_style, false,
                                                 flex_container_size.height(),
                                                 &depends_on_flex_container);
  EXPECT_FALSE(depends_on_flex_container);
  EXPECT_TRUE(used_flex_basis);
  EXPECT_EQ(*used_flex_basis, LayoutUnit(25));
}

TEST(UsedStyleTest, UsedWidthAutoDependsOnContainingBlock) {
  scoped_refptr<cssom::MutableCSSComputedStyleData> computed_style(
      new cssom::MutableCSSComputedStyleData());
  computed_style->set_width(cssom::KeywordValue::GetAuto());
  SizeLayoutUnit containing_block_size(LayoutUnit(200.0f), LayoutUnit(400.0f));
  bool depends_on_containing_block = false;
  auto used_width = GetUsedWidthIfNotAuto(computed_style, containing_block_size,
                                          &depends_on_containing_block);
  EXPECT_TRUE(depends_on_containing_block);
  EXPECT_FALSE(used_width);
}

TEST(UsedStyleTest, UsedWidthPercentDependsOnContainingBlockWidth) {
  scoped_refptr<cssom::MutableCSSComputedStyleData> computed_style(
      new cssom::MutableCSSComputedStyleData());
  computed_style->set_width(new cssom::PercentageValue(0.25f));
  SizeLayoutUnit containing_block_size(LayoutUnit(200.0f), LayoutUnit(400.0f));
  bool depends_on_containing_block = false;
  auto used_width = GetUsedWidthIfNotAuto(computed_style, containing_block_size,
                                          &depends_on_containing_block);
  EXPECT_TRUE(depends_on_containing_block);
  EXPECT_TRUE(used_width);
  EXPECT_EQ(*used_width, LayoutUnit(50.0f));
}

TEST(UsedStyleTest, UsedWidthLengthValue) {
  scoped_refptr<cssom::MutableCSSComputedStyleData> computed_style(
      new cssom::MutableCSSComputedStyleData());
  computed_style->set_width(new cssom::LengthValue(25.0f, cssom::kPixelsUnit));
  SizeLayoutUnit containing_block_size(LayoutUnit(200.0f), LayoutUnit(400.0f));
  bool depends_on_containing_block = false;
  auto used_width = GetUsedWidthIfNotAuto(computed_style, containing_block_size,
                                          &depends_on_containing_block);
  EXPECT_FALSE(depends_on_containing_block);
  EXPECT_TRUE(used_width);
  EXPECT_EQ(*used_width, LayoutUnit(25.0f));
}

TEST(UsedStyleTest, UsedMaxHeightNone) {
  scoped_refptr<cssom::MutableCSSComputedStyleData> computed_style(
      new cssom::MutableCSSComputedStyleData());
  computed_style->set_max_height(cssom::KeywordValue::GetNone());
  SizeLayoutUnit containing_block_size(LayoutUnit(200.0f), LayoutUnit(400.0f));
  auto used_max_height =
      GetUsedMaxHeightIfNotNone(computed_style, containing_block_size);
  EXPECT_FALSE(used_max_height);
}

TEST(UsedStyleTest, UsedMaxHeightPercentDependsOnContainingBlockHeight) {
  scoped_refptr<cssom::MutableCSSComputedStyleData> computed_style(
      new cssom::MutableCSSComputedStyleData());
  computed_style->set_max_height(new cssom::PercentageValue(0.25f));
  SizeLayoutUnit containing_block_size(LayoutUnit(200.0f), LayoutUnit(400.0f));
  auto used_max_height =
      GetUsedMaxHeightIfNotNone(computed_style, containing_block_size);
  EXPECT_TRUE(used_max_height);
  EXPECT_EQ(*used_max_height, LayoutUnit(100.0f));
}

TEST(UsedStyleTest, UsedMaxHeightLengthValue) {
  scoped_refptr<cssom::MutableCSSComputedStyleData> computed_style(
      new cssom::MutableCSSComputedStyleData());
  computed_style->set_max_height(
      new cssom::LengthValue(25.0f, cssom::kPixelsUnit));
  SizeLayoutUnit containing_block_size(LayoutUnit(200.0f), LayoutUnit(400.0f));
  auto used_max_height =
      GetUsedMaxHeightIfNotNone(computed_style, containing_block_size);
  EXPECT_TRUE(used_max_height);
  EXPECT_EQ(*used_max_height, LayoutUnit(25.0f));
}

TEST(UsedStyleTest, UsedMaxWidthNoneDependsOnContainingBlock) {
  scoped_refptr<cssom::MutableCSSComputedStyleData> computed_style(
      new cssom::MutableCSSComputedStyleData());
  computed_style->set_max_width(cssom::KeywordValue::GetNone());
  SizeLayoutUnit containing_block_size(LayoutUnit(200.0f), LayoutUnit(400.0f));
  bool depends_on_containing_block = false;
  auto used_max_width = GetUsedMaxWidthIfNotNone(
      computed_style, containing_block_size, &depends_on_containing_block);
  EXPECT_TRUE(depends_on_containing_block);
  EXPECT_FALSE(used_max_width);
}

TEST(UsedStyleTest, UsedMaxWidthPercentDependsOnContainingBlockWidth) {
  scoped_refptr<cssom::MutableCSSComputedStyleData> computed_style(
      new cssom::MutableCSSComputedStyleData());
  computed_style->set_max_width(new cssom::PercentageValue(0.25f));
  SizeLayoutUnit containing_block_size(LayoutUnit(200.0f), LayoutUnit(400.0f));
  bool depends_on_containing_block = false;
  auto used_max_width = GetUsedMaxWidthIfNotNone(
      computed_style, containing_block_size, &depends_on_containing_block);
  EXPECT_TRUE(depends_on_containing_block);
  EXPECT_TRUE(used_max_width);
  EXPECT_EQ(*used_max_width, LayoutUnit(50.0f));
}

TEST(UsedStyleTest, UsedMaxWidthLengthValue) {
  scoped_refptr<cssom::MutableCSSComputedStyleData> computed_style(
      new cssom::MutableCSSComputedStyleData());
  computed_style->set_max_width(
      new cssom::LengthValue(25.0f, cssom::kPixelsUnit));
  SizeLayoutUnit containing_block_size(LayoutUnit(200.0f), LayoutUnit(400.0f));
  bool depends_on_containing_block = false;
  auto used_max_width = GetUsedMaxWidthIfNotNone(
      computed_style, containing_block_size, &depends_on_containing_block);
  EXPECT_FALSE(depends_on_containing_block);
  EXPECT_TRUE(used_max_width);
  EXPECT_EQ(*used_max_width, LayoutUnit(25.0f));
}

TEST(UsedStyleTest, UsedMinHeightAuto) {
  scoped_refptr<cssom::MutableCSSComputedStyleData> computed_style(
      new cssom::MutableCSSComputedStyleData());
  computed_style->set_min_height(cssom::KeywordValue::GetAuto());
  SizeLayoutUnit containing_block_size(LayoutUnit(200.0f), LayoutUnit(400.0f));
  auto used_min_height =
      GetUsedMinHeightIfNotAuto(computed_style, containing_block_size);
  EXPECT_FALSE(used_min_height);
}

TEST(UsedStyleTest, UsedMinHeightPercentDependsOnContainingBlockHeight) {
  scoped_refptr<cssom::MutableCSSComputedStyleData> computed_style(
      new cssom::MutableCSSComputedStyleData());
  computed_style->set_min_height(new cssom::PercentageValue(0.25f));
  SizeLayoutUnit containing_block_size(LayoutUnit(200.0f), LayoutUnit(400.0f));
  auto used_min_height =
      GetUsedMinHeightIfNotAuto(computed_style, containing_block_size);
  EXPECT_TRUE(used_min_height);
  EXPECT_EQ(*used_min_height, LayoutUnit(100.0f));
}

TEST(UsedStyleTest, UsedMinHeightLengthValue) {
  scoped_refptr<cssom::MutableCSSComputedStyleData> computed_style(
      new cssom::MutableCSSComputedStyleData());
  computed_style->set_min_height(
      new cssom::LengthValue(25.0f, cssom::kPixelsUnit));
  SizeLayoutUnit containing_block_size(LayoutUnit(200.0f), LayoutUnit(400.0f));
  auto used_min_height =
      GetUsedMinHeightIfNotAuto(computed_style, containing_block_size);
  EXPECT_TRUE(used_min_height);
  EXPECT_EQ(*used_min_height, LayoutUnit(25.0f));
}

TEST(UsedStyleTest, UsedMinWidthAutoDoesNotDependOnContainingBlockWidth) {
  scoped_refptr<cssom::MutableCSSComputedStyleData> computed_style(
      new cssom::MutableCSSComputedStyleData());
  computed_style->set_min_width(cssom::KeywordValue::GetAuto());
  SizeLayoutUnit containing_block_size(LayoutUnit(200.0f), LayoutUnit(400.0f));
  bool depends_on_containing_block = false;
  auto used_min_width = GetUsedMinWidthIfNotAuto(
      computed_style, containing_block_size, &depends_on_containing_block);
  EXPECT_FALSE(depends_on_containing_block);
  EXPECT_FALSE(used_min_width);
}

TEST(UsedStyleTest, UsedMinWidthPercentDependsOnContainingBlockWidth) {
  scoped_refptr<cssom::MutableCSSComputedStyleData> computed_style(
      new cssom::MutableCSSComputedStyleData());
  computed_style->set_min_width(new cssom::PercentageValue(0.25f));
  SizeLayoutUnit containing_block_size(LayoutUnit(200.0f), LayoutUnit(400.0f));
  bool depends_on_containing_block = false;
  auto used_min_width = GetUsedMinWidthIfNotAuto(
      computed_style, containing_block_size, &depends_on_containing_block);
  EXPECT_TRUE(depends_on_containing_block);
  EXPECT_TRUE(used_min_width);
  EXPECT_EQ(*used_min_width, LayoutUnit(50.0f));
}

TEST(UsedStyleTest, UsedMinWidthLengthValue) {
  scoped_refptr<cssom::MutableCSSComputedStyleData> computed_style(
      new cssom::MutableCSSComputedStyleData());
  computed_style->set_min_width(
      new cssom::LengthValue(25.0f, cssom::kPixelsUnit));
  SizeLayoutUnit containing_block_size(LayoutUnit(200.0f), LayoutUnit(400.0f));
  bool depends_on_containing_block = false;
  auto used_min_width = GetUsedMinWidthIfNotAuto(
      computed_style, containing_block_size, &depends_on_containing_block);
  EXPECT_FALSE(depends_on_containing_block);
  EXPECT_TRUE(used_min_width);
  EXPECT_EQ(*used_min_width, LayoutUnit(25.0f));
}

TEST(UsedStyleTest, UsedHeightAutoDependsOnContainingBlock) {
  scoped_refptr<cssom::MutableCSSComputedStyleData> computed_style(
      new cssom::MutableCSSComputedStyleData());
  computed_style->set_height(cssom::KeywordValue::GetAuto());
  SizeLayoutUnit containing_block_size(LayoutUnit(200.0f), LayoutUnit(400.0f));
  bool depends_on_containing_block = false;
  auto used_height = GetUsedHeightIfNotAuto(
      computed_style, containing_block_size, &depends_on_containing_block);
  EXPECT_TRUE(depends_on_containing_block);
  EXPECT_FALSE(used_height);
}

TEST(UsedStyleTest, UsedHeightPercentDependsOnContainingBlockHeight) {
  scoped_refptr<cssom::MutableCSSComputedStyleData> computed_style(
      new cssom::MutableCSSComputedStyleData());
  computed_style->set_height(new cssom::PercentageValue(0.25f));
  SizeLayoutUnit containing_block_size(LayoutUnit(200.0f), LayoutUnit(400.0f));
  bool depends_on_containing_block = false;
  auto used_height = GetUsedHeightIfNotAuto(
      computed_style, containing_block_size, &depends_on_containing_block);
  EXPECT_TRUE(depends_on_containing_block);
  EXPECT_TRUE(used_height);
  EXPECT_EQ(*used_height, LayoutUnit(100.0f));
}

TEST(UsedStyleTest, UsedHeightLengthValue) {
  scoped_refptr<cssom::MutableCSSComputedStyleData> computed_style(
      new cssom::MutableCSSComputedStyleData());
  computed_style->set_height(new cssom::LengthValue(25.0f, cssom::kPixelsUnit));
  SizeLayoutUnit containing_block_size(LayoutUnit(200.0f), LayoutUnit(400.0f));
  bool depends_on_containing_block = true;
  auto used_height = GetUsedHeightIfNotAuto(
      computed_style, containing_block_size, &depends_on_containing_block);
  EXPECT_FALSE(depends_on_containing_block);
  EXPECT_TRUE(used_height);
  EXPECT_EQ(*used_height, LayoutUnit(25.0f));
}

}  // namespace layout
}  // namespace cobalt
