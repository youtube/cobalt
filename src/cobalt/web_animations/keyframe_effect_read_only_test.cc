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

#include "cobalt/web_animations/keyframe_effect_read_only.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/number_value.h"
#include "cobalt/cssom/property_definitions.h"
#include "cobalt/cssom/rgba_color_value.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace web_animations {

TEST(KeyframeEffectReadOnlyDataTest, IsPropertyAnimated) {
  std::vector<Keyframe::Data> keyframes;

  Keyframe::Data frame1(0.0);
  frame1.AddPropertyValue(cssom::kColorProperty,
                          new cssom::RGBAColorValue(0, 100, 0, 100));
  keyframes.push_back(frame1);

  Keyframe::Data frame2(0.5);
  frame2.AddPropertyValue(cssom::kOpacityProperty, new cssom::NumberValue(0.5));
  keyframes.push_back(frame2);

  Keyframe::Data frame3(1.0);
  frame3.AddPropertyValue(cssom::kBackgroundColorProperty,
                          new cssom::RGBAColorValue(0, 100, 0, 100));
  frame3.AddPropertyValue(cssom::kTopProperty,
                          new cssom::LengthValue(1.0f, cssom::kPixelsUnit));
  keyframes.push_back(frame3);

  KeyframeEffectReadOnly::Data keyframe_effect(keyframes);

  // Properties that are referenced by keyframes should be reported as being
  // animated.
  EXPECT_TRUE(keyframe_effect.IsPropertyAnimated(cssom::kColorProperty));
  EXPECT_TRUE(keyframe_effect.IsPropertyAnimated(cssom::kOpacityProperty));
  EXPECT_TRUE(
      keyframe_effect.IsPropertyAnimated(cssom::kBackgroundColorProperty));
  EXPECT_TRUE(keyframe_effect.IsPropertyAnimated(cssom::kTopProperty));

  // Properties we did not reference in keyframes should not be reported as
  // being animated.
  EXPECT_FALSE(keyframe_effect.IsPropertyAnimated(cssom::kLeftProperty));
  EXPECT_FALSE(keyframe_effect.IsPropertyAnimated(cssom::kFontSizeProperty));
  EXPECT_FALSE(keyframe_effect.IsPropertyAnimated(cssom::kTransformProperty));
}

namespace {
std::vector<Keyframe::Data> AddNoiseKeyframes(
    const std::vector<Keyframe::Data>& keyframes) {
  // Adds noise keyframes to the set of keyframes.  Noise here means other
  // keyframes with a "NULL" target property so that they are guaranteed to
  // not match any property that any of these tests wish to animate.
  // Additionally, existing keyframes will have a "NULL" target property added
  // to them.
  std::vector<Keyframe::Data> noisy_keyframes;

  // Add an initial noise keyframe at offset 0 to start the sequence.
  Keyframe::Data noise_keyframe_begin(0.0);
  noise_keyframe_begin.AddPropertyValue(cssom::kNoneProperty,
                                        new cssom::NumberValue(0));
  noisy_keyframes.push_back(noise_keyframe_begin);

  for (std::vector<Keyframe::Data>::const_iterator iter = keyframes.begin();
       iter != keyframes.end(); ++iter) {
    Keyframe::Data noise_keyframe(*iter->offset());
    noise_keyframe.AddPropertyValue(cssom::kNoneProperty,
                                    new cssom::NumberValue(0));

    // Add one noise keyframe before the real keyframe
    noisy_keyframes.push_back(noise_keyframe);

    // Add the real keyframe, with a noise property added to it.
    Keyframe::Data real_keyframe = *iter;
    real_keyframe.AddPropertyValue(cssom::kNoneProperty,
                                   new cssom::NumberValue(0));
    noisy_keyframes.push_back(real_keyframe);

    // Add one noise keyframe after the real keyframe.
    noisy_keyframes.push_back(noise_keyframe);
  }

  // Add a final noise keyframe at offset 1 to end the sequence.
  Keyframe::Data noise_keyframe_end(1.0);
  noise_keyframe_end.AddPropertyValue(cssom::kNoneProperty,
                                      new cssom::NumberValue(0));
  noisy_keyframes.push_back(noise_keyframe_end);

  return noisy_keyframes;
}

KeyframeEffectReadOnly::Data CreateKeyframeEffectWithTwoNumberKeyframes(
    cssom::PropertyKey target_property, bool add_noise_keyframes,
    double offset1, float value1, double offset2, float value2) {
  std::vector<Keyframe::Data> keyframes;

  Keyframe::Data frame1(offset1);
  frame1.AddPropertyValue(target_property, new cssom::NumberValue(value1));
  keyframes.push_back(frame1);

  Keyframe::Data frame2(offset2);
  frame2.AddPropertyValue(target_property, new cssom::NumberValue(value2));
  keyframes.push_back(frame2);

  return KeyframeEffectReadOnly::Data(
      add_noise_keyframes ? AddNoiseKeyframes(keyframes) : keyframes);
}

KeyframeEffectReadOnly::Data CreateKeyframeEffectWithThreeNumberKeyframes(
    cssom::PropertyKey target_property, bool add_noise_keyframes,
    double offset1, float value1, double offset2, float value2, double offset3,
    float value3) {
  std::vector<Keyframe::Data> keyframes;

  Keyframe::Data frame1(offset1);
  frame1.AddPropertyValue(target_property, new cssom::NumberValue(value1));
  keyframes.push_back(frame1);

  Keyframe::Data frame2(offset2);
  frame2.AddPropertyValue(target_property, new cssom::NumberValue(value2));
  keyframes.push_back(frame2);

  Keyframe::Data frame3(offset3);
  frame3.AddPropertyValue(target_property, new cssom::NumberValue(value3));
  keyframes.push_back(frame3);

  return KeyframeEffectReadOnly::Data(
      add_noise_keyframes ? AddNoiseKeyframes(keyframes) : keyframes);
}

template <typename T>
scoped_refptr<T> ComputeAnimatedPropertyValueTyped(
    const KeyframeEffectReadOnly::Data& effect,
    cssom::PropertyKey target_property,
    const scoped_refptr<cssom::PropertyValue>& underlying_value,
    double iteration_progress, double current_iteration) {
  scoped_refptr<cssom::PropertyValue> animated =
      effect.ComputeAnimatedPropertyValue(target_property, underlying_value,
                                          iteration_progress,
                                          current_iteration);
  scoped_refptr<T> animated_typed = dynamic_cast<T*>(animated.get());
  DCHECK(animated_typed);

  return animated_typed;
}
}  // namespace

// We parameterize the following suite of tests on whether or not "noise
// keyframes" are to be included in the tests.  Since all of these tests will
// be targeting a single property, "noise keyframes" means that the keyframe
// effects will contain keyframes for properties that are not our target
// property, and the keyframes that do contain our target property will also
// refer to our noise property.  Essentially, all test results need to be
// the same regardless of whether noise is present or not, as when animating
// a single property, we only care about keyframes that contain that property.
class KeyframeEffectReadOnlyDataSingleParameterKeyframeTests
    : public ::testing::TestWithParam<bool> {};

TEST_P(KeyframeEffectReadOnlyDataSingleParameterKeyframeTests,
       NoPropertySpecificKeyframes) {
  // If we create a keyframe effect that animates opacity, then applying it
  // to all other properties should simply return the underlying value for
  // the other properties.
  KeyframeEffectReadOnly::Data effect =
      CreateKeyframeEffectWithTwoNumberKeyframes(
          cssom::kOpacityProperty, GetParam(), 0.0, 0.25f, 1.0, 0.75f);

  scoped_refptr<cssom::NumberValue> underlying_value =
      new cssom::NumberValue(0.1f);

  scoped_refptr<cssom::NumberValue> animated =
      ComputeAnimatedPropertyValueTyped<cssom::NumberValue>(
          effect, cssom::kLeftProperty, underlying_value, 0.5, 0);

  EXPECT_TRUE(animated->Equals(*underlying_value));
}

TEST_P(KeyframeEffectReadOnlyDataSingleParameterKeyframeTests,
       Offset0AutoPopulatesWithUnderlyingValue) {
  // If our keyframe effect does not specify a value for offset 0, it should
  // be created automatically from the underlying value.
  KeyframeEffectReadOnly::Data effect =
      CreateKeyframeEffectWithTwoNumberKeyframes(
          cssom::kOpacityProperty, GetParam(), 0.5, 0.8f, 1.0, 1.0f);

  scoped_refptr<cssom::NumberValue> underlying_value =
      new cssom::NumberValue(0.1f);

  scoped_refptr<cssom::NumberValue> animated =
      ComputeAnimatedPropertyValueTyped<cssom::NumberValue>(
          effect, cssom::kOpacityProperty, underlying_value, 0.25, 0);

  EXPECT_FLOAT_EQ(0.45f, animated->value());
}

TEST_P(KeyframeEffectReadOnlyDataSingleParameterKeyframeTests,
       Offset1AutoPopulatesWithUnderlyingValue) {
  // If our keyframe effect does not specify a value for offset 0, it should
  // be created automatically from the underlying value.
  KeyframeEffectReadOnly::Data effect =
      CreateKeyframeEffectWithTwoNumberKeyframes(
          cssom::kOpacityProperty, GetParam(), 0.0, 0.0f, 0.5, 0.8f);

  scoped_refptr<cssom::NumberValue> underlying_value =
      new cssom::NumberValue(0.1f);

  scoped_refptr<cssom::NumberValue> animated =
      ComputeAnimatedPropertyValueTyped<cssom::NumberValue>(
          effect, cssom::kOpacityProperty, underlying_value, 0.75, 0);

  EXPECT_FLOAT_EQ(0.45f, animated->value());
}

TEST_P(KeyframeEffectReadOnlyDataSingleParameterKeyframeTests,
       NegativeIterationProgressReturnsFirstOffset0Value) {
  // If iteration progress is less than 0 and we have multiple keyframes with
  // offset 0, we should return the value of the first of these.
  // Step 10 from:
  //   https://www.w3.org/TR/2015/WD-web-animations-1-20150707/#the-effect-value-of-a-keyframe-animation-effect
  KeyframeEffectReadOnly::Data effect =
      CreateKeyframeEffectWithTwoNumberKeyframes(
          cssom::kOpacityProperty, GetParam(), 0.0, 0.2f, 0.0, 0.8f);

  scoped_refptr<cssom::NumberValue> underlying_value =
      new cssom::NumberValue(0.1f);

  scoped_refptr<cssom::NumberValue> animated =
      ComputeAnimatedPropertyValueTyped<cssom::NumberValue>(
          effect, cssom::kOpacityProperty, underlying_value, -1.0, 0);

  EXPECT_FLOAT_EQ(0.2f, animated->value());
}

TEST_P(KeyframeEffectReadOnlyDataSingleParameterKeyframeTests,
       IterationProgressGreaterThan1ReturnsLastOffset1Value) {
  // If iteration progress is greater than 1 and we have multiple keyframes with
  // offset 1, we should return the value of the last of these.
  // Step 10 from:
  //   https://www.w3.org/TR/2015/WD-web-animations-1-20150707/#the-effect-value-of-a-keyframe-animation-effect
  KeyframeEffectReadOnly::Data effect =
      CreateKeyframeEffectWithTwoNumberKeyframes(
          cssom::kOpacityProperty, GetParam(), 1.0, 0.2f, 1.0, 0.8f);

  scoped_refptr<cssom::NumberValue> underlying_value =
      new cssom::NumberValue(0.1f);

  scoped_refptr<cssom::NumberValue> animated =
      ComputeAnimatedPropertyValueTyped<cssom::NumberValue>(
          effect, cssom::kOpacityProperty, underlying_value, 2.0, 0);

  EXPECT_FLOAT_EQ(0.8f, animated->value());
}

TEST_P(KeyframeEffectReadOnlyDataSingleParameterKeyframeTests,
       IterationProgressLessThan0Extrapolates) {
  // If iteration progress is greater than 1, we should extrapolate from the
  // last keyframe.
  KeyframeEffectReadOnly::Data effect =
      CreateKeyframeEffectWithTwoNumberKeyframes(
          cssom::kOpacityProperty, GetParam(), 0.0, 0.2f, 1.0, 0.8f);

  scoped_refptr<cssom::NumberValue> underlying_value =
      new cssom::NumberValue(0.1f);

  scoped_refptr<cssom::NumberValue> animated =
      ComputeAnimatedPropertyValueTyped<cssom::NumberValue>(
          effect, cssom::kOpacityProperty, underlying_value, -1.0, 0);

  EXPECT_FLOAT_EQ(-0.4f, animated->value());
}

TEST_P(KeyframeEffectReadOnlyDataSingleParameterKeyframeTests,
       IterationProgressLessThan0ExtrapolatesWithoutExplicit0OffsetKeyframe) {
  // If iteration progress is greater than 1, we should extrapolate from the
  // last keyframe.
  KeyframeEffectReadOnly::Data effect =
      CreateKeyframeEffectWithTwoNumberKeyframes(
          cssom::kOpacityProperty, GetParam(), 0.5, 0.2f, 1.0, 0.8f);

  scoped_refptr<cssom::NumberValue> underlying_value =
      new cssom::NumberValue(0.1f);

  scoped_refptr<cssom::NumberValue> animated =
      ComputeAnimatedPropertyValueTyped<cssom::NumberValue>(
          effect, cssom::kOpacityProperty, underlying_value, -1.0, 0);

  EXPECT_FLOAT_EQ(-0.1f, animated->value());
}

TEST_P(KeyframeEffectReadOnlyDataSingleParameterKeyframeTests,
       IterationProgressGreaterThan1Extrapolates) {
  // If iteration progress is greater than 1, we should extrapolate from the
  // last keyframe.
  KeyframeEffectReadOnly::Data effect =
      CreateKeyframeEffectWithTwoNumberKeyframes(
          cssom::kOpacityProperty, GetParam(), 0.0, 0.2f, 1.0, 0.8f);

  scoped_refptr<cssom::NumberValue> underlying_value =
      new cssom::NumberValue(0.1f);

  scoped_refptr<cssom::NumberValue> animated =
      ComputeAnimatedPropertyValueTyped<cssom::NumberValue>(
          effect, cssom::kOpacityProperty, underlying_value, 2.0, 0);

  EXPECT_FLOAT_EQ(1.4f, animated->value());
}

TEST_P(
    KeyframeEffectReadOnlyDataSingleParameterKeyframeTests,
    IterationProgressGreaterThan1ExtrapolatesWithoutExplicit1OffsetKeyframe) {
  // If iteration progress is greater than 1, we should extrapolate from the
  // last keyframe.
  KeyframeEffectReadOnly::Data effect =
      CreateKeyframeEffectWithTwoNumberKeyframes(
          cssom::kOpacityProperty, GetParam(), 0.0, 0.2f, 0.5, 0.8f);

  scoped_refptr<cssom::NumberValue> underlying_value =
      new cssom::NumberValue(0.9f);

  scoped_refptr<cssom::NumberValue> animated =
      ComputeAnimatedPropertyValueTyped<cssom::NumberValue>(
          effect, cssom::kOpacityProperty, underlying_value, 2.0, 0);

  EXPECT_FLOAT_EQ(1.1f, animated->value());
}

TEST_P(KeyframeEffectReadOnlyDataSingleParameterKeyframeTests,
       LastKeyframeWithSameOffsetIsChosenAsFirstEndpoint) {
  // If we have two keyframes with the same offset, and that offset is chosen
  // as the offset of the first interval endpoint, then we use the last keyframe
  // with the given offset.
  KeyframeEffectReadOnly::Data effect =
      CreateKeyframeEffectWithTwoNumberKeyframes(
          cssom::kOpacityProperty, GetParam(), 0.5, 0.2f, 0.5, 0.8f);

  scoped_refptr<cssom::NumberValue> underlying_value =
      new cssom::NumberValue(0.9f);

  scoped_refptr<cssom::NumberValue> animated =
      ComputeAnimatedPropertyValueTyped<cssom::NumberValue>(
          effect, cssom::kOpacityProperty, underlying_value, 0.75, 0);

  EXPECT_FLOAT_EQ(0.85f, animated->value());
}

TEST_P(KeyframeEffectReadOnlyDataSingleParameterKeyframeTests,
       KeyframeAfterFirstIntervalEndpointIsSecondIntervalEndpoint) {
  // Check that the very next keyframe after our first interval endpoint is
  // the one chosen as the second interval endpoint.  E.g., if we have two
  // candidates for the second endpoint with the same offset, the first of
  // those is chosen.
  KeyframeEffectReadOnly::Data effect =
      CreateKeyframeEffectWithTwoNumberKeyframes(
          cssom::kOpacityProperty, GetParam(), 0.5, 0.2f, 0.5, 0.8f);

  scoped_refptr<cssom::NumberValue> underlying_value =
      new cssom::NumberValue(0.1f);

  scoped_refptr<cssom::NumberValue> animated =
      ComputeAnimatedPropertyValueTyped<cssom::NumberValue>(
          effect, cssom::kOpacityProperty, underlying_value, 0.25, 0);

  EXPECT_FLOAT_EQ(0.15f, animated->value());
}

TEST_P(KeyframeEffectReadOnlyDataSingleParameterKeyframeTests,
       AllIntervalsOfMultiIntervalEffectEvaluateCorrectly) {
  // Check that all 4 intervals in a 3-keyframe effect evaluate to the correct
  // values.
  KeyframeEffectReadOnly::Data effect =
      CreateKeyframeEffectWithThreeNumberKeyframes(cssom::kOpacityProperty,
                                                   GetParam(), 0.25, 0.1f, 0.5,
                                                   0.5f, 0.75, 0.9f);

  scoped_refptr<cssom::NumberValue> underlying_value =
      new cssom::NumberValue(0.0f);

  scoped_refptr<cssom::NumberValue> animated1 =
      ComputeAnimatedPropertyValueTyped<cssom::NumberValue>(
          effect, cssom::kOpacityProperty, underlying_value, 0.125, 0);
  EXPECT_FLOAT_EQ(0.05f, animated1->value());

  scoped_refptr<cssom::NumberValue> animated2 =
      ComputeAnimatedPropertyValueTyped<cssom::NumberValue>(
          effect, cssom::kOpacityProperty, underlying_value, 0.375, 0);
  EXPECT_FLOAT_EQ(0.3f, animated2->value());

  scoped_refptr<cssom::NumberValue> animated3 =
      ComputeAnimatedPropertyValueTyped<cssom::NumberValue>(
          effect, cssom::kOpacityProperty, underlying_value, 0.625, 0);
  EXPECT_FLOAT_EQ(0.7f, animated3->value());

  scoped_refptr<cssom::NumberValue> animated4 =
      ComputeAnimatedPropertyValueTyped<cssom::NumberValue>(
          effect, cssom::kOpacityProperty, underlying_value, 0.875, 0);
  EXPECT_FLOAT_EQ(0.45f, animated4->value());
}

INSTANTIATE_TEST_CASE_P(WithAndWithoutNoise,
                        KeyframeEffectReadOnlyDataSingleParameterKeyframeTests,
                        ::testing::Bool());

}  // namespace web_animations
}  // namespace cobalt
