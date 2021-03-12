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

#include <cmath>

#include "base/time/time.h"
#include "cobalt/cssom/calc_value.h"
#include "cobalt/cssom/cobalt_ui_nav_focus_transform_function.h"
#include "cobalt/cssom/cobalt_ui_nav_spotlight_transform_function.h"
#include "cobalt/cssom/css_transition.h"
#include "cobalt/cssom/interpolate_property_value.h"
#include "cobalt/cssom/interpolated_transform_property_value.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/matrix_function.h"
#include "cobalt/cssom/number_value.h"
#include "cobalt/cssom/property_definitions.h"
#include "cobalt/cssom/rgba_color_value.h"
#include "cobalt/cssom/rotate_function.h"
#include "cobalt/cssom/scale_function.h"
#include "cobalt/cssom/transform_function_list_value.h"
#include "cobalt/cssom/translate_function.h"
#include "cobalt/math/transform_2d.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace cssom {

namespace {
const float kErrorEpsilon = 0.00015f;

// Helper function to animate a propertyby a given progress between a given
// start and end point.  The animated value is returned, casted to the passed in
// template type parameter.
template <typename T>
scoped_refptr<T> InterpolatePropertyTyped(
    float progress, const scoped_refptr<PropertyValue>& start,
    const scoped_refptr<PropertyValue>& end) {
  scoped_refptr<PropertyValue> interpolated =
      InterpolatePropertyValue(progress, start, end);

  scoped_refptr<T> interpolated_with_type =
      dynamic_cast<T*>(interpolated.get());
  DCHECK(interpolated_with_type);

  return interpolated_with_type;
}
}  // namespace

TEST(InterpolatePropertyValueTest, LengthValuesInterpolate) {
  scoped_refptr<LengthValue> interpolated =
      InterpolatePropertyTyped<LengthValue>(0.5f,
                                            new LengthValue(0.0f, kPixelsUnit),
                                            new LengthValue(4.0f, kPixelsUnit));

  EXPECT_NEAR(interpolated->value(), 2.0f, kErrorEpsilon);
  EXPECT_EQ(interpolated->unit(), kPixelsUnit);
}

TEST(InterpolatePropertyValueTest, NumberValuesInterpolate) {
  scoped_refptr<NumberValue> interpolated =
      InterpolatePropertyTyped<NumberValue>(0.5f, new NumberValue(0.0f),
                                            new NumberValue(4.0f));

  EXPECT_NEAR(interpolated->value(), 2.0f, kErrorEpsilon);
}

TEST(InterpolatePropertyValueTest, RGBAColorValuesInterpolate) {
  scoped_refptr<RGBAColorValue> interpolated =
      InterpolatePropertyTyped<RGBAColorValue>(
          0.25, new RGBAColorValue(0, 100, 0, 100),
          new RGBAColorValue(100, 0, 100, 0));

  EXPECT_EQ(25, interpolated->r());
  EXPECT_EQ(75, interpolated->g());
  EXPECT_EQ(25, interpolated->b());
  EXPECT_EQ(75, interpolated->a());
}

TEST(InterpolatePropertyValueTest, TransformFromNoneToNoneValuesInterpolate) {
  scoped_refptr<KeywordValue> interpolated =
      InterpolatePropertyTyped<KeywordValue>(0.5f, KeywordValue::GetNone(),
                                             KeywordValue::GetNone());

  EXPECT_EQ(KeywordValue::kNone, interpolated->value());
}

TEST(InterpolatePropertyValueTest, TransformSingleRotateValuesInterpolate) {
  struct MakeSingleRotateTransform {
    static scoped_refptr<PropertyValue> Start() {
      TransformFunctionListValue::Builder functions;
      functions.emplace_back(new RotateFunction(1.0f));
      return new TransformFunctionListValue(std::move(functions));
    }
    static scoped_refptr<PropertyValue> End() {
      TransformFunctionListValue::Builder functions;
      functions.emplace_back(new RotateFunction(2.0f));
      return new TransformFunctionListValue(std::move(functions));
    }
  };

  scoped_refptr<TransformFunctionListValue> interpolated =
      InterpolatePropertyTyped<TransformFunctionListValue>(
          0.5f, MakeSingleRotateTransform::Start(),
          MakeSingleRotateTransform::End());

  ASSERT_EQ(1, interpolated->value().size());

  const RotateFunction* single_function =
      dynamic_cast<const RotateFunction*>(interpolated->value()[0].get());
  ASSERT_TRUE(single_function);

  EXPECT_NEAR(single_function->clockwise_angle_in_radians(), 1.5f,
              kErrorEpsilon);
}

TEST(InterpolatePropertyValueTest,
     TransformFromNoneToCobaltUiNavFocusTransformValuesInterpolate) {
  struct MakeSingleFocusTransform {
    static scoped_refptr<PropertyValue> Start() {
      return KeywordValue::GetNone();
    }
    static scoped_refptr<PropertyValue> End() {
      TransformFunctionListValue::Builder functions;
      functions.emplace_back(new CobaltUiNavFocusTransformFunction(1.0f, 1.0f));
      return new TransformFunctionListValue(std::move(functions));
    }
  };

  scoped_refptr<TransformFunctionListValue> interpolated =
      InterpolatePropertyTyped<TransformFunctionListValue>(
          0.75f, MakeSingleFocusTransform::Start(),
          MakeSingleFocusTransform::End());

  ASSERT_EQ(1, interpolated->value().size());

  const CobaltUiNavFocusTransformFunction* focus_function =
      dynamic_cast<const CobaltUiNavFocusTransformFunction*>(
          interpolated->value()[0].get());
  ASSERT_TRUE(focus_function);
  EXPECT_NEAR(focus_function->x_translation_scale(), 1.0f, kErrorEpsilon);
  EXPECT_NEAR(focus_function->y_translation_scale(), 1.0f, kErrorEpsilon);
  EXPECT_NEAR(focus_function->progress_to_identity(), 0.25f, kErrorEpsilon);

  math::Matrix3F value = focus_function->ToMatrix(math::SizeF(), nullptr);
  EXPECT_NEAR(value(0, 0), 1.0f, kErrorEpsilon);
  EXPECT_NEAR(value(1, 1), 1.0f, kErrorEpsilon);
  EXPECT_NEAR(value(0, 2), 0.0f, kErrorEpsilon);
  EXPECT_NEAR(value(1, 2), 0.0f, kErrorEpsilon);
}

TEST(InterpolatePropertyValueTest,
     TransformSingleCobaltUiNavFocusTransformValuesInterpolate) {
  struct MakeSingleFocusTransform {
    static scoped_refptr<PropertyValue> Start() {
      TransformFunctionListValue::Builder functions;
      functions.emplace_back(
          new CobaltUiNavFocusTransformFunction(1.0f, 2.0f, 0.2f));
      return new TransformFunctionListValue(std::move(functions));
    }
    static scoped_refptr<PropertyValue> End() {
      TransformFunctionListValue::Builder functions;
      functions.emplace_back(
          new CobaltUiNavFocusTransformFunction(2.0f, 4.0f, 0.6f));
      return new TransformFunctionListValue(std::move(functions));
    }
  };

  scoped_refptr<TransformFunctionListValue> interpolated =
      InterpolatePropertyTyped<TransformFunctionListValue>(
          0.5f, MakeSingleFocusTransform::Start(),
          MakeSingleFocusTransform::End());

  ASSERT_EQ(1, interpolated->value().size());

  const CobaltUiNavFocusTransformFunction* focus_function =
      dynamic_cast<const CobaltUiNavFocusTransformFunction*>(
          interpolated->value()[0].get());
  ASSERT_TRUE(focus_function);
  EXPECT_NEAR(focus_function->x_translation_scale(), 1.5f, kErrorEpsilon);
  EXPECT_NEAR(focus_function->y_translation_scale(), 3.0f, kErrorEpsilon);
  EXPECT_NEAR(focus_function->progress_to_identity(), 0.4f, kErrorEpsilon);

  math::Matrix3F value = focus_function->ToMatrix(math::SizeF(), nullptr);
  EXPECT_NEAR(value(0, 0), 1.0f, kErrorEpsilon);
  EXPECT_NEAR(value(1, 1), 1.0f, kErrorEpsilon);
  EXPECT_NEAR(value(0, 2), 0.0f, kErrorEpsilon);
  EXPECT_NEAR(value(1, 2), 0.0f, kErrorEpsilon);
}

TEST(InterpolatePropertyValueTest,
     TransformFromNoneToCobaltUiNavSpotlightTransformValuesInterpolate) {
  struct MakeSingleSpotlightTransform {
    static scoped_refptr<PropertyValue> Start() {
      return KeywordValue::GetNone();
    }
    static scoped_refptr<PropertyValue> End() {
      TransformFunctionListValue::Builder functions;
      functions.emplace_back(new CobaltUiNavSpotlightTransformFunction);
      return new TransformFunctionListValue(std::move(functions));
    }
  };

  scoped_refptr<TransformFunctionListValue> interpolated =
      InterpolatePropertyTyped<TransformFunctionListValue>(
          0.25f, MakeSingleSpotlightTransform::Start(),
          MakeSingleSpotlightTransform::End());

  ASSERT_EQ(1, interpolated->value().size());

  const CobaltUiNavSpotlightTransformFunction* spotlight_function =
      dynamic_cast<const CobaltUiNavSpotlightTransformFunction*>(
          interpolated->value()[0].get());
  ASSERT_TRUE(spotlight_function);
  EXPECT_NEAR(spotlight_function->progress_to_identity(), 0.75f, kErrorEpsilon);

  math::Matrix3F value = spotlight_function->ToMatrix(math::SizeF(), nullptr);
  EXPECT_NEAR(value(0, 0), 0.75f, kErrorEpsilon);
  EXPECT_NEAR(value(1, 1), 0.75f, kErrorEpsilon);
  EXPECT_NEAR(value(0, 2), 0.0f, kErrorEpsilon);
  EXPECT_NEAR(value(1, 2), 0.0f, kErrorEpsilon);
}

TEST(InterpolatePropertyValueTest,
     TransformSingleCobaltUiNavSpotlightTransformValuesInterpolate) {
  struct MakeSingleSpotlightTransform {
    static scoped_refptr<PropertyValue> Start() {
      TransformFunctionListValue::Builder functions;
      functions.emplace_back(new CobaltUiNavSpotlightTransformFunction(0.0f));
      return new TransformFunctionListValue(std::move(functions));
    }
    static scoped_refptr<PropertyValue> End() {
      TransformFunctionListValue::Builder functions;
      functions.emplace_back(new CobaltUiNavSpotlightTransformFunction(0.5f));
      return new TransformFunctionListValue(std::move(functions));
    }
  };

  scoped_refptr<TransformFunctionListValue> interpolated =
      InterpolatePropertyTyped<TransformFunctionListValue>(
          0.5f, MakeSingleSpotlightTransform::Start(),
          MakeSingleSpotlightTransform::End());

  ASSERT_EQ(1, interpolated->value().size());

  const CobaltUiNavSpotlightTransformFunction* spotlight_function =
      dynamic_cast<const CobaltUiNavSpotlightTransformFunction*>(
          interpolated->value()[0].get());
  ASSERT_TRUE(spotlight_function);
  EXPECT_NEAR(spotlight_function->progress_to_identity(), 0.25f, kErrorEpsilon);

  math::Matrix3F value = spotlight_function->ToMatrix(math::SizeF(), nullptr);
  EXPECT_NEAR(value(0, 0), 0.25f, kErrorEpsilon);
  EXPECT_NEAR(value(1, 1), 0.25f, kErrorEpsilon);
  EXPECT_NEAR(value(0, 2), 0.0f, kErrorEpsilon);
  EXPECT_NEAR(value(1, 2), 0.0f, kErrorEpsilon);
}

TEST(InterpolatePropertyValueTest, TransformSingleScaleValuesInterpolate) {
  struct MakeSingleScaleTransform {
    static scoped_refptr<PropertyValue> Start() {
      TransformFunctionListValue::Builder functions;
      functions.emplace_back(new ScaleFunction(1.0f, 2.0f));
      return new TransformFunctionListValue(std::move(functions));
    }
    static scoped_refptr<PropertyValue> End() {
      TransformFunctionListValue::Builder functions;
      functions.emplace_back(new ScaleFunction(3.0f, 4.0f));
      return new TransformFunctionListValue(std::move(functions));
    }
  };

  scoped_refptr<TransformFunctionListValue> interpolated =
      InterpolatePropertyTyped<TransformFunctionListValue>(
          0.5f, MakeSingleScaleTransform::Start(),
          MakeSingleScaleTransform::End());

  ASSERT_EQ(1, interpolated->value().size());

  const ScaleFunction* single_function =
      dynamic_cast<const ScaleFunction*>(interpolated->value()[0].get());
  ASSERT_TRUE(single_function);

  EXPECT_NEAR(single_function->x_factor(), 2.0f, kErrorEpsilon);
  EXPECT_NEAR(single_function->y_factor(), 3.0f, kErrorEpsilon);
}

TEST(InterpolatePropertyValueTest, TransformFromNoneToScaleValuesInterpolate) {
  struct MakeSingleScaleTransform {
    static scoped_refptr<PropertyValue> Start() {
      return KeywordValue::GetNone();
    }
    static scoped_refptr<PropertyValue> End() {
      TransformFunctionListValue::Builder functions;
      functions.emplace_back(new ScaleFunction(5.0f, 9.0f));
      return new TransformFunctionListValue(std::move(functions));
    }
  };

  scoped_refptr<TransformFunctionListValue> interpolated =
      InterpolatePropertyTyped<TransformFunctionListValue>(
          0.25f, MakeSingleScaleTransform::Start(),
          MakeSingleScaleTransform::End());

  ASSERT_EQ(1, interpolated->value().size());

  const ScaleFunction* single_function =
      dynamic_cast<const ScaleFunction*>(interpolated->value()[0].get());
  ASSERT_TRUE(single_function);

  EXPECT_NEAR(single_function->x_factor(), 2.0f, kErrorEpsilon);
  EXPECT_NEAR(single_function->y_factor(), 3.0f, kErrorEpsilon);
}

TEST(InterpolatePropertyValueTest, TransformFromScaleToNoneValuesInterpolate) {
  struct MakeSingleScaleTransform {
    static scoped_refptr<PropertyValue> Start() {
      TransformFunctionListValue::Builder functions;
      functions.emplace_back(new ScaleFunction(5.0f, 9.0f));
      return new TransformFunctionListValue(std::move(functions));
    }
    static scoped_refptr<PropertyValue> End() {
      return KeywordValue::GetNone();
    }
  };

  scoped_refptr<TransformFunctionListValue> interpolated =
      InterpolatePropertyTyped<TransformFunctionListValue>(
          0.25f, MakeSingleScaleTransform::Start(),
          MakeSingleScaleTransform::End());

  ASSERT_EQ(1, interpolated->value().size());

  const ScaleFunction* single_function =
      dynamic_cast<const ScaleFunction*>(interpolated->value()[0].get());
  ASSERT_TRUE(single_function);

  EXPECT_NEAR(single_function->x_factor(), 4.0f, kErrorEpsilon);
  EXPECT_NEAR(single_function->y_factor(), 7.0f, kErrorEpsilon);
}

void TestTransformSingleTranslateValuesAnimate(TranslateFunction::Axis axis) {
  struct MakeSingleTranslateTransform {
    static scoped_refptr<PropertyValue> Start(TranslateFunction::Axis axis) {
      TransformFunctionListValue::Builder functions;
      functions.emplace_back(
          new TranslateFunction(axis, new LengthValue(1.0f, kPixelsUnit)));
      return new TransformFunctionListValue(std::move(functions));
    }
    static scoped_refptr<PropertyValue> End(TranslateFunction::Axis axis) {
      TransformFunctionListValue::Builder functions;
      functions.emplace_back(
          new TranslateFunction(axis, new LengthValue(2.0f, kPixelsUnit)));
      return new TransformFunctionListValue(std::move(functions));
    }
  };

  scoped_refptr<TransformFunctionListValue> interpolated =
      InterpolatePropertyTyped<TransformFunctionListValue>(
          0.5f, MakeSingleTranslateTransform::Start(axis),
          MakeSingleTranslateTransform::End(axis));

  ASSERT_EQ(1, interpolated->value().size());
  const TranslateFunction* single_function =
      dynamic_cast<const TranslateFunction*>(interpolated->value()[0].get());
  ASSERT_TRUE(single_function);

  EXPECT_NEAR(single_function->offset_as_length()->value(), 1.5f,
              kErrorEpsilon);
  EXPECT_EQ(kPixelsUnit, single_function->offset_as_length()->unit());
  EXPECT_EQ(axis, single_function->axis());
}

TEST(InterpolatePropertyValueTest, TransformSingleTranslateXValuesInterpolate) {
  TestTransformSingleTranslateValuesAnimate(TranslateFunction::kXAxis);
}
TEST(InterpolatePropertyValueTest, TransformSingleTranslateYValuesInterpolate) {
  TestTransformSingleTranslateValuesAnimate(TranslateFunction::kYAxis);
}
TEST(InterpolatePropertyValueTest, TransformSingleTranslateZValuesInterpolate) {
  TestTransformSingleTranslateValuesAnimate(TranslateFunction::kZAxis);
}

void TestTransformTranslateFromLengthToPercentageValuesAnimate(
    TranslateFunction::Axis axis) {
  struct MakeSingleTranslateTransform {
    static scoped_refptr<PropertyValue> Start(TranslateFunction::Axis axis) {
      TransformFunctionListValue::Builder functions;
      functions.emplace_back(
          new TranslateFunction(axis, new LengthValue(1.0f, kPixelsUnit)));
      return new TransformFunctionListValue(std::move(functions));
    }
    static scoped_refptr<PropertyValue> End(TranslateFunction::Axis axis) {
      TransformFunctionListValue::Builder functions;
      functions.emplace_back(
          new TranslateFunction(axis, new PercentageValue(0.5f)));
      return new TransformFunctionListValue(std::move(functions));
    }
  };

  scoped_refptr<TransformFunctionListValue> interpolated =
      InterpolatePropertyTyped<TransformFunctionListValue>(
          0.5f, MakeSingleTranslateTransform::Start(axis),
          MakeSingleTranslateTransform::End(axis));

  ASSERT_EQ(1, interpolated->value().size());
  const TranslateFunction* single_function =
      dynamic_cast<const TranslateFunction*>(interpolated->value()[0].get());
  ASSERT_TRUE(single_function);

  ASSERT_EQ(TranslateFunction::kCalc, single_function->offset_type());
  EXPECT_NEAR(0.5f, single_function->offset_as_calc()->length_value()->value(),
              kErrorEpsilon);
  EXPECT_EQ(kPixelsUnit,
            single_function->offset_as_calc()->length_value()->unit());
  EXPECT_NEAR(0.25f,
              single_function->offset_as_calc()->percentage_value()->value(),
              kErrorEpsilon);
  EXPECT_EQ(axis, single_function->axis());
}

TEST(InterpolatePropertyValueTest,
     TransformTranslateXFromLengthToPercentageValuesAnimate) {
  TestTransformTranslateFromLengthToPercentageValuesAnimate(
      TranslateFunction::kXAxis);
}
TEST(InterpolatePropertyValueTest,
     TransformTranslateYFromLengthToPercentageValuesAnimate) {
  TestTransformTranslateFromLengthToPercentageValuesAnimate(
      TranslateFunction::kYAxis);
}
TEST(InterpolatePropertyValueTest,
     TransformTranslateZFromLengthToPercentageValuesAnimate) {
  TestTransformTranslateFromLengthToPercentageValuesAnimate(
      TranslateFunction::kZAxis);
}

void TestTransformTranslateCalcValuesAnimate(TranslateFunction::Axis axis) {
  struct MakeSingleTranslateTransform {
    static scoped_refptr<PropertyValue> Start(TranslateFunction::Axis axis) {
      TransformFunctionListValue::Builder functions;
      functions.emplace_back(new TranslateFunction(
          axis, new CalcValue(new LengthValue(1.0f, kPixelsUnit),
                              new PercentageValue(0.2f))));
      return new TransformFunctionListValue(std::move(functions));
    }
    static scoped_refptr<PropertyValue> End(TranslateFunction::Axis axis) {
      TransformFunctionListValue::Builder functions;
      functions.emplace_back(new TranslateFunction(
          axis, new CalcValue(new LengthValue(2.0f, kPixelsUnit),
                              new PercentageValue(0.4f))));
      return new TransformFunctionListValue(std::move(functions));
    }
  };

  scoped_refptr<TransformFunctionListValue> interpolated =
      InterpolatePropertyTyped<TransformFunctionListValue>(
          0.5f, MakeSingleTranslateTransform::Start(axis),
          MakeSingleTranslateTransform::End(axis));

  ASSERT_EQ(1, interpolated->value().size());
  const TranslateFunction* single_function =
      dynamic_cast<const TranslateFunction*>(interpolated->value()[0].get());
  ASSERT_TRUE(single_function);

  ASSERT_EQ(TranslateFunction::kCalc, single_function->offset_type());
  EXPECT_NEAR(1.5f, single_function->offset_as_calc()->length_value()->value(),
              kErrorEpsilon);
  EXPECT_EQ(kPixelsUnit,
            single_function->offset_as_calc()->length_value()->unit());
  EXPECT_NEAR(0.3f,
              single_function->offset_as_calc()->percentage_value()->value(),
              kErrorEpsilon);
  EXPECT_EQ(axis, single_function->axis());
}

TEST(InterpolatePropertyValueTest, TestTransformTranslateXCalcValuesAnimate) {
  TestTransformTranslateCalcValuesAnimate(TranslateFunction::kXAxis);
}
TEST(InterpolatePropertyValueTest, TestTransformTranslateYCalcValuesAnimate) {
  TestTransformTranslateCalcValuesAnimate(TranslateFunction::kYAxis);
}
TEST(InterpolatePropertyValueTest, TestTransformTranslateZCalcValuesAnimate) {
  TestTransformTranslateCalcValuesAnimate(TranslateFunction::kZAxis);
}

void TestTransformFromTranslateToNoneValuesAnimate(
    TranslateFunction::Axis axis) {
  struct MakeSingleTranslateTransform {
    static scoped_refptr<PropertyValue> Start(TranslateFunction::Axis axis) {
      TransformFunctionListValue::Builder functions;
      functions.emplace_back(
          new TranslateFunction(axis, new LengthValue(1.0f, kPixelsUnit)));
      return new TransformFunctionListValue(std::move(functions));
    }
    static scoped_refptr<PropertyValue> End() {
      return KeywordValue::GetNone();
    }
  };

  scoped_refptr<TransformFunctionListValue> interpolated =
      InterpolatePropertyTyped<TransformFunctionListValue>(
          0.5f, MakeSingleTranslateTransform::Start(axis),
          MakeSingleTranslateTransform::End());

  ASSERT_EQ(1, interpolated->value().size());
  const TranslateFunction* single_function =
      dynamic_cast<const TranslateFunction*>(interpolated->value()[0].get());
  ASSERT_TRUE(single_function);

  EXPECT_NEAR(single_function->offset_as_length()->value(), 0.5f,
              kErrorEpsilon);
  EXPECT_EQ(kPixelsUnit, single_function->offset_as_length()->unit());
  EXPECT_EQ(axis, single_function->axis());
}

TEST(InterpolatePropertyValueTest,
     TransformFromTranslateXToNoneValuesInterpolate) {
  TestTransformFromTranslateToNoneValuesAnimate(TranslateFunction::kXAxis);
}
TEST(InterpolatePropertyValueTest,
     TransformFromTranslateYToNoneValuesInterpolate) {
  TestTransformFromTranslateToNoneValuesAnimate(TranslateFunction::kYAxis);
}
TEST(InterpolatePropertyValueTest,
     TransformFromTranslateZToNoneValuesInterpolate) {
  TestTransformFromTranslateToNoneValuesAnimate(TranslateFunction::kZAxis);
}

TEST(InterpolatePropertyValueTest,
     CanAnimateTransformListWithMultipleElementsButSameSizeAndTypes) {
  struct MakeSingleScaleTransform {
    static scoped_refptr<PropertyValue> Start() {
      TransformFunctionListValue::Builder functions;
      functions.emplace_back(new RotateFunction(1.0f));
      functions.emplace_back(new ScaleFunction(1.0f, 2.0f));
      functions.emplace_back(new TranslateFunction(
          TranslateFunction::kXAxis, new LengthValue(3.0f, kPixelsUnit)));
      functions.emplace_back(new TranslateFunction(
          TranslateFunction::kYAxis, new LengthValue(4.0f, kPixelsUnit)));
      functions.emplace_back(new TranslateFunction(
          TranslateFunction::kZAxis, new LengthValue(5.0f, kPixelsUnit)));
      return new TransformFunctionListValue(std::move(functions));
    }
    static scoped_refptr<PropertyValue> End() {
      TransformFunctionListValue::Builder functions;
      functions.emplace_back(new RotateFunction(2.0f));
      functions.emplace_back(new ScaleFunction(7.0f, 8.0f));
      functions.emplace_back(new TranslateFunction(
          TranslateFunction::kXAxis, new LengthValue(9.0f, kPixelsUnit)));
      functions.emplace_back(new TranslateFunction(
          TranslateFunction::kYAxis, new LengthValue(10.0f, kPixelsUnit)));
      functions.emplace_back(new TranslateFunction(
          TranslateFunction::kZAxis, new LengthValue(11.0f, kPixelsUnit)));
      return new TransformFunctionListValue(std::move(functions));
    }
  };

  scoped_refptr<TransformFunctionListValue> interpolated =
      InterpolatePropertyTyped<TransformFunctionListValue>(
          0.5f, MakeSingleScaleTransform::Start(),
          MakeSingleScaleTransform::End());

  ASSERT_EQ(5, interpolated->value().size());

  const RotateFunction* first_function =
      dynamic_cast<const RotateFunction*>(interpolated->value()[0].get());
  ASSERT_TRUE(first_function);
  EXPECT_NEAR(first_function->clockwise_angle_in_radians(), 1.5f,
              kErrorEpsilon);

  const ScaleFunction* second_function =
      dynamic_cast<const ScaleFunction*>(interpolated->value()[1].get());
  ASSERT_TRUE(second_function);
  EXPECT_NEAR(second_function->x_factor(), 4.0f, kErrorEpsilon);
  EXPECT_NEAR(second_function->y_factor(), 5.0f, kErrorEpsilon);

  const TranslateFunction* third_function =
      dynamic_cast<const TranslateFunction*>(interpolated->value()[2].get());
  ASSERT_TRUE(third_function);
  EXPECT_NEAR(third_function->offset_as_length()->value(), 6.0f, kErrorEpsilon);
  EXPECT_EQ(kPixelsUnit, third_function->offset_as_length()->unit());

  const TranslateFunction* fourth_function =
      dynamic_cast<const TranslateFunction*>(interpolated->value()[3].get());
  ASSERT_TRUE(fourth_function);
  EXPECT_NEAR(fourth_function->offset_as_length()->value(), 7.0f,
              kErrorEpsilon);
  EXPECT_EQ(kPixelsUnit, fourth_function->offset_as_length()->unit());

  const TranslateFunction* fifth_function =
      dynamic_cast<const TranslateFunction*>(interpolated->value()[4].get());
  ASSERT_TRUE(fifth_function);
  EXPECT_NEAR(fifth_function->offset_as_length()->value(), 8.0f, kErrorEpsilon);
  EXPECT_EQ(kPixelsUnit, fifth_function->offset_as_length()->unit());
}

TEST(InterpolatePropertyValueTest,
     TransformSingleRotationMatrixValuesInterpolate) {
  struct MakeSingleMatrixTransform {
    static scoped_refptr<PropertyValue> Start() {
      TransformFunctionListValue::Builder functions;
      functions.emplace_back(new MatrixFunction(math::RotateMatrix(0.0f)));
      return new TransformFunctionListValue(std::move(functions));
    }
    static scoped_refptr<PropertyValue> End() {
      TransformFunctionListValue::Builder functions;
      functions.emplace_back(
          new MatrixFunction(math::RotateMatrix(static_cast<float>(M_PI / 2))));
      return new TransformFunctionListValue(std::move(functions));
    }
  };

  scoped_refptr<TransformFunctionListValue> interpolated =
      InterpolatePropertyTyped<TransformFunctionListValue>(
          0.5f, MakeSingleMatrixTransform::Start(),
          MakeSingleMatrixTransform::End());

  ASSERT_EQ(1, interpolated->value().size());

  const MatrixFunction* single_function =
      dynamic_cast<const MatrixFunction*>(interpolated->value()[0].get());
  ASSERT_TRUE(single_function);

  EXPECT_TRUE(single_function->value().IsNear(
      math::RotateMatrix(static_cast<float>(M_PI / 4)), kErrorEpsilon));
}

TEST(InterpolatePropertyValueTest,
     TransformSingleScaleMatrixValuesInterpolate) {
  struct MakeSingleMatrixTransform {
    static scoped_refptr<PropertyValue> Start() {
      TransformFunctionListValue::Builder functions;
      functions.emplace_back(new MatrixFunction(math::ScaleMatrix(2.0f, 1.0f)));
      return new TransformFunctionListValue(std::move(functions));
    }
    static scoped_refptr<PropertyValue> End() {
      TransformFunctionListValue::Builder functions;
      functions.emplace_back(new MatrixFunction(math::ScaleMatrix(4.0f, 2.0f)));
      return new TransformFunctionListValue(std::move(functions));
    }
  };

  scoped_refptr<TransformFunctionListValue> interpolated =
      InterpolatePropertyTyped<TransformFunctionListValue>(
          0.5f, MakeSingleMatrixTransform::Start(),
          MakeSingleMatrixTransform::End());

  ASSERT_EQ(1, interpolated->value().size());

  const MatrixFunction* single_function =
      dynamic_cast<const MatrixFunction*>(interpolated->value()[0].get());
  ASSERT_TRUE(single_function);

  EXPECT_TRUE(single_function->value().IsNear(math::ScaleMatrix(3.0f, 1.5f),
                                              kErrorEpsilon));
}

TEST(InterpolatePropertyValueTest,
     TransformSingleTranslateMatrixValuesInterpolate) {
  struct MakeSingleMatrixTransform {
    static scoped_refptr<PropertyValue> Start() {
      TransformFunctionListValue::Builder functions;
      functions.emplace_back(
          new MatrixFunction(math::TranslateMatrix(2.0f, 1.0f)));
      return new TransformFunctionListValue(std::move(functions));
    }
    static scoped_refptr<PropertyValue> End() {
      TransformFunctionListValue::Builder functions;
      functions.emplace_back(
          new MatrixFunction(math::TranslateMatrix(4.0f, 2.0f)));
      return new TransformFunctionListValue(std::move(functions));
    }
  };

  scoped_refptr<TransformFunctionListValue> interpolated =
      InterpolatePropertyTyped<TransformFunctionListValue>(
          0.5f, MakeSingleMatrixTransform::Start(),
          MakeSingleMatrixTransform::End());

  ASSERT_EQ(1, interpolated->value().size());

  const MatrixFunction* single_function =
      dynamic_cast<const MatrixFunction*>(interpolated->value()[0].get());
  ASSERT_TRUE(single_function);

  EXPECT_TRUE(single_function->value().IsNear(math::TranslateMatrix(3.0f, 1.5f),
                                              kErrorEpsilon));
}

TEST(InterpolatePropertyValueTest,
     MultipleMismatchedTransformValuesInterpolate) {
  struct MakeMultipleMismatchedTransform {
    static scoped_refptr<PropertyValue> Start() {
      TransformFunctionListValue::Builder functions;
      functions.emplace_back(new TranslateFunction(
          TranslateFunction::kXAxis, new LengthValue(2.0f, kPixelsUnit)));
      functions.emplace_back(new ScaleFunction(2.0f, 1.0f));
      return new TransformFunctionListValue(std::move(functions));
    }
    static scoped_refptr<PropertyValue> End() {
      TransformFunctionListValue::Builder functions;
      functions.emplace_back(new TranslateFunction(
          TranslateFunction::kXAxis, new LengthValue(4.0f, kPixelsUnit)));
      functions.emplace_back(new RotateFunction(static_cast<float>(M_PI / 2)));
      functions.emplace_back(new ScaleFunction(4.0f, 2.0f));
      return new TransformFunctionListValue(std::move(functions));
    }
  };

  // Since the original transform list had mismatched types, the result should
  // be interpolated by matrix.
  scoped_refptr<InterpolatedTransformPropertyValue> interpolated =
      InterpolatePropertyTyped<InterpolatedTransformPropertyValue>(
          0.75f, MakeMultipleMismatchedTransform::Start(),
          MakeMultipleMismatchedTransform::End());
  EXPECT_TRUE(
      interpolated->ToMatrix(math::SizeF(), nullptr)
          .IsNear(math::TranslateMatrix(3.5f, 0.0f) *
                      math::RotateMatrix(-static_cast<float>(M_PI * 3 / 8)) *
                      math::ScaleMatrix(3.5f, 1.75f),
                  kErrorEpsilon));
}

TEST(InterpolatePropertyValueTest,
     RotationTransformMatrixTransitionsFromTransitions) {
  // Transition from a rotation transformation of 45 degrees to a translate
  // transformation of 0 (i.e. identity).  Since they are of different types,
  // we will invoke the "decompose to a matrix" path of transitioning a
  // transform, and then we c an simply test for the correct rotation value.
  struct MakeMultipleMismatchedTransform {
    static scoped_refptr<PropertyValue> Start() {
      TransformFunctionListValue::Builder functions;
      functions.emplace_back(new TranslateFunction(
          TranslateFunction::kXAxis, new LengthValue(0.0f, kPixelsUnit)));
      return new TransformFunctionListValue(std::move(functions));
    }
    static scoped_refptr<PropertyValue> End() {
      TransformFunctionListValue::Builder functions;
      functions.emplace_back(new RotateFunction(static_cast<float>(M_PI / 2)));
      return new TransformFunctionListValue(std::move(functions));
    }
  };

  // Since the original transform list had mismatched types, the result should
  // be interpolated by matrix.
  scoped_refptr<InterpolatedTransformPropertyValue> interpolated =
      InterpolatePropertyTyped<InterpolatedTransformPropertyValue>(
          0.5f, MakeMultipleMismatchedTransform::Start(),
          MakeMultipleMismatchedTransform::End());

  scoped_refptr<InterpolatedTransformPropertyValue> next_interpolated =
      InterpolatePropertyTyped<InterpolatedTransformPropertyValue>(
          0.5f, interpolated, MakeMultipleMismatchedTransform::Start());

  EXPECT_TRUE(next_interpolated->ToMatrix(math::SizeF(), nullptr)
                  .IsNear(math::RotateMatrix(-static_cast<float>(M_PI / 8)),
                          kErrorEpsilon));
}

TEST(InterpolatePropertyValueTest,
     CanInterpolateFromTranslateLengthToTranslatePercentage) {
  struct MakeMultipleMismatchedTransform {
    static scoped_refptr<PropertyValue> Start() {
      TransformFunctionListValue::Builder functions;
      functions.emplace_back(new TranslateFunction(
          TranslateFunction::kXAxis, new LengthValue(5.0f, kPixelsUnit)));
      return new TransformFunctionListValue(std::move(functions));
    }
    static scoped_refptr<PropertyValue> End() {
      TransformFunctionListValue::Builder functions;
      functions.emplace_back(new TranslateFunction(TranslateFunction::kXAxis,
                                                   new PercentageValue(0.5f)));
      return new TransformFunctionListValue(std::move(functions));
    }
  };

  scoped_refptr<TransformFunctionListValue> interpolated =
      InterpolatePropertyTyped<TransformFunctionListValue>(
          0.5f, MakeMultipleMismatchedTransform::Start(),
          MakeMultipleMismatchedTransform::End());

  ASSERT_EQ(1, interpolated->value().size());
  const TranslateFunction* single_function =
      dynamic_cast<const TranslateFunction*>(interpolated->value()[0].get());
  ASSERT_TRUE(single_function);
  EXPECT_EQ(TranslateFunction::kXAxis, single_function->axis());

  scoped_refptr<CalcValue> calc_value = single_function->offset_as_calc();
  EXPECT_NEAR(2.5f, calc_value->length_value()->value(), kErrorEpsilon);
  EXPECT_EQ(kPixelsUnit, calc_value->length_value()->unit());
  EXPECT_NEAR(0.25f, calc_value->percentage_value()->value(), kErrorEpsilon);
}

TEST(InterpolatePropertyValueTest,
     CanInterpolateMatricesWithPercentageAndOffsets) {
  struct MakeMultipleMismatchedTransform {
    static scoped_refptr<PropertyValue> Start() {
      TransformFunctionListValue::Builder functions;
      functions.emplace_back(new TranslateFunction(
          TranslateFunction::kXAxis,
          new CalcValue(new LengthValue(2.0f, kPixelsUnit),
                        new PercentageValue(0.2f))));
      functions.emplace_back(new RotateFunction(static_cast<float>(M_PI / 2)));
      functions.emplace_back(new TranslateFunction(
          TranslateFunction::kXAxis,
          new CalcValue(new LengthValue(10.0f, kPixelsUnit),
                        new PercentageValue(1.0f))));
      return new TransformFunctionListValue(std::move(functions));
    }
    static scoped_refptr<PropertyValue> End() {
      TransformFunctionListValue::Builder functions;
      functions.emplace_back(new TranslateFunction(
          TranslateFunction::kYAxis,
          new CalcValue(new LengthValue(5.0f, kPixelsUnit),
                        new PercentageValue(0.5f))));
      return new TransformFunctionListValue(std::move(functions));
    }
  };

  // Since the original transform list had mismatched types, the result should
  // be interpolated by matrix.
  scoped_refptr<InterpolatedTransformPropertyValue> interpolated =
      InterpolatePropertyTyped<InterpolatedTransformPropertyValue>(
          0.5f, MakeMultipleMismatchedTransform::Start(),
          MakeMultipleMismatchedTransform::End());

  math::Matrix3F value =
      interpolated->ToMatrix(math::SizeF(100.0f, 200.0f), nullptr);

  EXPECT_NEAR(cos(M_PI / 4), value(0, 0), kErrorEpsilon);
  EXPECT_NEAR(sin(M_PI / 4), value(1, 0), kErrorEpsilon);
  EXPECT_NEAR(-sin(M_PI / 4), value(0, 1), kErrorEpsilon);
  EXPECT_NEAR(cos(M_PI / 4), value(1, 1), kErrorEpsilon);
  EXPECT_NEAR(11.0f, value(0, 2), kErrorEpsilon);
  EXPECT_NEAR(107.5f, value(1, 2), kErrorEpsilon);
}

}  // namespace cssom
}  // namespace cobalt
