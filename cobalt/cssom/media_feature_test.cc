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

#include "cobalt/cssom/media_feature.h"

#include "base/memory/ref_counted.h"
#include "cobalt/cssom/integer_value.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/media_feature_keyword_value.h"
#include "cobalt/cssom/media_feature_names.h"
#include "cobalt/cssom/ratio_value.h"
#include "cobalt/cssom/resolution_value.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace cssom {
namespace {

TEST(MediaFeatureTest, AspectRatioNonZeroShouldEvaluateTrue) {
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kAspectRatioMediaFeature));
  media_feature->set_operator(kNonZero);

  EXPECT_TRUE(media_feature->EvaluateConditionValue(ViewportSize(1920, 1080)));
}

TEST(MediaFeatureTest, AspectRatioEqualsShouldEvaluateTrue) {
  scoped_refptr<RatioValue> property(new RatioValue(math::Rational(16, 9)));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kAspectRatioMediaFeature, property));
  media_feature->set_operator(kEquals);

  EXPECT_TRUE(media_feature->EvaluateConditionValue(ViewportSize(1920, 1080)));
}

TEST(MediaFeatureTest, AspectRatioEqualsShouldEvaluateFalse) {
  scoped_refptr<RatioValue> property(new RatioValue(math::Rational(16, 9)));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kAspectRatioMediaFeature, property));
  media_feature->set_operator(kEquals);

  EXPECT_FALSE(media_feature->EvaluateConditionValue(ViewportSize(640, 480)));
}

TEST(MediaFeatureTest, AspectRatioMinimumShouldEvaluateTrue) {
  scoped_refptr<RatioValue> property(new RatioValue(math::Rational(4, 3)));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kAspectRatioMediaFeature, property));
  media_feature->set_operator(kMinimum);

  EXPECT_TRUE(media_feature->EvaluateConditionValue(ViewportSize(1920, 1080)));
}

TEST(MediaFeatureTest, AspectRatioMinimumShouldEvaluateFalse) {
  scoped_refptr<RatioValue> property(new RatioValue(math::Rational(16, 9)));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kAspectRatioMediaFeature, property));
  media_feature->set_operator(kMinimum);

  EXPECT_FALSE(media_feature->EvaluateConditionValue(ViewportSize(640, 480)));
}

TEST(MediaFeatureTest, AspectRatioMaximumShouldEvaluateTrue) {
  scoped_refptr<RatioValue> property(new RatioValue(math::Rational(16, 9)));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kAspectRatioMediaFeature, property));
  media_feature->set_operator(kMaximum);

  EXPECT_TRUE(media_feature->EvaluateConditionValue(ViewportSize(640, 480)));
}

TEST(MediaFeatureTest, AspectRatioMaximumShouldEvaluateFalse) {
  scoped_refptr<RatioValue> property(new RatioValue(math::Rational(4, 3)));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kAspectRatioMediaFeature, property));
  media_feature->set_operator(kMaximum);

  EXPECT_FALSE(media_feature->EvaluateConditionValue(ViewportSize(1920, 1080)));
}

TEST(MediaFeatureTest, ColorIndexNonZeroShouldEvaluateFalse) {
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kColorIndexMediaFeature));
  media_feature->set_operator(kNonZero);
  EXPECT_FALSE(media_feature->EvaluateConditionValue(ViewportSize(0, 0)));
}

TEST(MediaFeatureTest, ColorIndexEqualsShouldEvaluateTrue) {
  scoped_refptr<IntegerValue> property(new IntegerValue(0));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kColorIndexMediaFeature, property));
  media_feature->set_operator(kEquals);
  EXPECT_TRUE(media_feature->EvaluateConditionValue(ViewportSize(0, 0)));
}

TEST(MediaFeatureTest, ColorIndexEqualsShouldEvaluateFalse) {
  scoped_refptr<IntegerValue> property(new IntegerValue(256));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kColorIndexMediaFeature, property));
  media_feature->set_operator(kEquals);
  EXPECT_FALSE(media_feature->EvaluateConditionValue(ViewportSize(0, 0)));
}

TEST(MediaFeatureTest, ColorIndexMinimumShouldEvaluateTrue) {
  scoped_refptr<IntegerValue> property(new IntegerValue(0));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kColorIndexMediaFeature, property));
  media_feature->set_operator(kMinimum);
  EXPECT_TRUE(media_feature->EvaluateConditionValue(ViewportSize(0, 0)));
}

TEST(MediaFeatureTest, ColorIndexMinimumShouldEvaluateFalse) {
  scoped_refptr<IntegerValue> property(new IntegerValue(256));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kColorIndexMediaFeature, property));
  media_feature->set_operator(kMinimum);
  EXPECT_FALSE(media_feature->EvaluateConditionValue(ViewportSize(0, 0)));
}

TEST(MediaFeatureTest, ColorIndexMaximumShouldEvaluateTrue) {
  scoped_refptr<IntegerValue> property(new IntegerValue(0));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kColorIndexMediaFeature, property));
  media_feature->set_operator(kMaximum);
  EXPECT_TRUE(media_feature->EvaluateConditionValue(ViewportSize(0, 0)));
}

TEST(MediaFeatureTest, ColorNonZeroShouldEvaluateTrue) {
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kColorMediaFeature));
  media_feature->set_operator(kNonZero);
  EXPECT_TRUE(media_feature->EvaluateConditionValue(ViewportSize(0, 0)));
}

TEST(MediaFeatureTest, ColorEqualsShouldEvaluateTrue) {
  scoped_refptr<IntegerValue> property(new IntegerValue(8));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kColorMediaFeature, property));
  media_feature->set_operator(kEquals);
  EXPECT_TRUE(media_feature->EvaluateConditionValue(ViewportSize(0, 0)));
}

TEST(MediaFeatureTest, ColorEqualsShouldEvaluateFalse) {
  scoped_refptr<IntegerValue> property(new IntegerValue(12));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kColorMediaFeature, property));
  media_feature->set_operator(kEquals);
  EXPECT_FALSE(media_feature->EvaluateConditionValue(ViewportSize(0, 0)));
}

TEST(MediaFeatureTest, ColorMinimumShouldEvaluateTrue) {
  scoped_refptr<IntegerValue> property(new IntegerValue(8));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kColorMediaFeature, property));
  media_feature->set_operator(kMinimum);
  EXPECT_TRUE(media_feature->EvaluateConditionValue(ViewportSize(0, 0)));
}

TEST(MediaFeatureTest, ColorMinimumShouldEvaluateFalse) {
  scoped_refptr<IntegerValue> property(new IntegerValue(12));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kColorMediaFeature, property));
  media_feature->set_operator(kMinimum);
  EXPECT_FALSE(media_feature->EvaluateConditionValue(ViewportSize(0, 0)));
}

TEST(MediaFeatureTest, ColorMaximumShouldEvaluateTrue) {
  scoped_refptr<IntegerValue> property(new IntegerValue(8));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kColorMediaFeature, property));
  media_feature->set_operator(kMaximum);
  EXPECT_TRUE(media_feature->EvaluateConditionValue(ViewportSize(0, 0)));
}

TEST(MediaFeatureTest, ColorMaximumShouldEvaluateFalse) {
  scoped_refptr<IntegerValue> property(new IntegerValue(1));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kColorMediaFeature, property));
  media_feature->set_operator(kMaximum);
  EXPECT_FALSE(media_feature->EvaluateConditionValue(ViewportSize(0, 0)));
}

TEST(MediaFeatureTest, DeviceAspectRatioNonZeroShouldEvaluateTrue) {
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kDeviceAspectRatioMediaFeature));
  media_feature->set_operator(kNonZero);

  EXPECT_TRUE(media_feature->EvaluateConditionValue(ViewportSize(1920, 1080)));
}

TEST(MediaFeatureTest, DeviceAspectRatioEqualsShouldEvaluateTrue) {
  scoped_refptr<RatioValue> property(new RatioValue(math::Rational(16, 9)));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kDeviceAspectRatioMediaFeature, property));
  media_feature->set_operator(kEquals);

  EXPECT_TRUE(media_feature->EvaluateConditionValue(ViewportSize(1920, 1080)));
}

TEST(MediaFeatureTest, DeviceAspectRatioEqualsShouldEvaluateFalse) {
  scoped_refptr<RatioValue> property(new RatioValue(math::Rational(16, 9)));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kDeviceAspectRatioMediaFeature, property));
  media_feature->set_operator(kEquals);

  EXPECT_FALSE(media_feature->EvaluateConditionValue(ViewportSize(640, 480)));
}

TEST(MediaFeatureTest, DeviceAspectRatioMinimumShouldEvaluateTrue) {
  scoped_refptr<RatioValue> property(new RatioValue(math::Rational(4, 3)));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kDeviceAspectRatioMediaFeature, property));
  media_feature->set_operator(kMinimum);

  EXPECT_TRUE(media_feature->EvaluateConditionValue(ViewportSize(1920, 1080)));
}

TEST(MediaFeatureTest, DeviceAspectRatioMinimumShouldEvaluateFalse) {
  scoped_refptr<RatioValue> property(new RatioValue(math::Rational(16, 9)));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kDeviceAspectRatioMediaFeature, property));
  media_feature->set_operator(kMinimum);

  EXPECT_FALSE(media_feature->EvaluateConditionValue(ViewportSize(640, 480)));
}

TEST(MediaFeatureTest, DeviceAspectRatioMaximumShouldEvaluateTrue) {
  scoped_refptr<RatioValue> property(new RatioValue(math::Rational(16, 9)));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kDeviceAspectRatioMediaFeature, property));
  media_feature->set_operator(kMaximum);

  EXPECT_TRUE(media_feature->EvaluateConditionValue(ViewportSize(640, 480)));
}

TEST(MediaFeatureTest, DeviceAspectRatioMaximumShouldEvaluateFalse) {
  scoped_refptr<RatioValue> property(new RatioValue(math::Rational(4, 3)));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kDeviceAspectRatioMediaFeature, property));
  media_feature->set_operator(kMaximum);

  EXPECT_FALSE(media_feature->EvaluateConditionValue(ViewportSize(1920, 1080)));
}

TEST(MediaFeatureTest, DeviceHeightNonZeroShouldEvaluateTrue) {
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kDeviceHeightMediaFeature));
  media_feature->set_operator(kNonZero);

  EXPECT_TRUE(media_feature->EvaluateConditionValue(ViewportSize(0, 1080)));
}

TEST(MediaFeatureTest, DeviceHeightZeroShouldEvaluateFalse) {
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kDeviceHeightMediaFeature));
  media_feature->set_operator(kNonZero);

  EXPECT_FALSE(media_feature->EvaluateConditionValue(ViewportSize(0, 0)));
}

TEST(MediaFeatureTest, DeviceHeightEqualsShouldEvaluateTrue) {
  scoped_refptr<LengthValue> property(new LengthValue(1080, kPixelsUnit));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kDeviceHeightMediaFeature, property));
  media_feature->set_operator(kEquals);

  EXPECT_TRUE(media_feature->EvaluateConditionValue(ViewportSize(0, 1080)));
}

TEST(MediaFeatureTest, DeviceHeightEqualsShouldEvaluateFalse) {
  scoped_refptr<LengthValue> property(new LengthValue(1080, kPixelsUnit));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kDeviceHeightMediaFeature, property));
  media_feature->set_operator(kEquals);

  EXPECT_FALSE(media_feature->EvaluateConditionValue(ViewportSize(0, 480)));
}

TEST(MediaFeatureTest, DeviceHeightMinimumShouldEvaluateTrue) {
  scoped_refptr<LengthValue> property(new LengthValue(720, kPixelsUnit));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kDeviceHeightMediaFeature, property));
  media_feature->set_operator(kMinimum);

  EXPECT_TRUE(media_feature->EvaluateConditionValue(ViewportSize(0, 1080)));
}

TEST(MediaFeatureTest, DeviceHeightMinimumShouldEvaluateFalse) {
  scoped_refptr<LengthValue> property(new LengthValue(1080, kPixelsUnit));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kDeviceHeightMediaFeature, property));
  media_feature->set_operator(kMinimum);

  EXPECT_FALSE(media_feature->EvaluateConditionValue(ViewportSize(0, 720)));
}

TEST(MediaFeatureTest, DeviceHeightMaximumShouldEvaluateTrue) {
  scoped_refptr<LengthValue> property(new LengthValue(720, kPixelsUnit));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kDeviceHeightMediaFeature, property));
  media_feature->set_operator(kMaximum);

  EXPECT_TRUE(media_feature->EvaluateConditionValue(ViewportSize(0, 480)));
}

TEST(MediaFeatureTest, DeviceHeightMaximumShouldEvaluateFalse) {
  scoped_refptr<LengthValue> property(new LengthValue(720, kPixelsUnit));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kDeviceHeightMediaFeature, property));
  media_feature->set_operator(kMaximum);

  EXPECT_FALSE(media_feature->EvaluateConditionValue(ViewportSize(0, 1080)));
}

TEST(MediaFeatureTest, DeviceWidthNonZeroShouldEvaluateTrue) {
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kDeviceWidthMediaFeature));
  media_feature->set_operator(kNonZero);

  EXPECT_TRUE(media_feature->EvaluateConditionValue(ViewportSize(1920, 0)));
}

TEST(MediaFeatureTest, DeviceWidthNonZeroShouldEvaluateFalse) {
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kDeviceWidthMediaFeature));
  media_feature->set_operator(kNonZero);

  EXPECT_FALSE(media_feature->EvaluateConditionValue(ViewportSize(0, 0)));
}

TEST(MediaFeatureTest, DeviceWidthEqualsShouldEvaluateTrue) {
  scoped_refptr<LengthValue> property(new LengthValue(1920, kPixelsUnit));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kDeviceWidthMediaFeature, property));
  media_feature->set_operator(kEquals);

  EXPECT_TRUE(media_feature->EvaluateConditionValue(ViewportSize(1920, 0)));
}

TEST(MediaFeatureTest, DeviceWidthEqualsShouldEvaluateFalse) {
  scoped_refptr<LengthValue> property(new LengthValue(1920, kPixelsUnit));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kDeviceWidthMediaFeature, property));
  media_feature->set_operator(kEquals);

  EXPECT_FALSE(media_feature->EvaluateConditionValue(ViewportSize(640, 0)));
}

TEST(MediaFeatureTest, DeviceWidthMinimumShouldEvaluateTrue) {
  scoped_refptr<LengthValue> property(new LengthValue(1280, kPixelsUnit));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kDeviceWidthMediaFeature, property));
  media_feature->set_operator(kMinimum);

  EXPECT_TRUE(media_feature->EvaluateConditionValue(ViewportSize(1920, 0)));
}

TEST(MediaFeatureTest, DeviceWidthMinimumShouldEvaluateFalse) {
  scoped_refptr<LengthValue> property(new LengthValue(1920, kPixelsUnit));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kDeviceWidthMediaFeature, property));
  media_feature->set_operator(kMinimum);

  EXPECT_FALSE(media_feature->EvaluateConditionValue(ViewportSize(1280, 0)));
}

TEST(MediaFeatureTest, DeviceWidthMaximumShouldEvaluateTrue) {
  scoped_refptr<LengthValue> property(new LengthValue(1280, kPixelsUnit));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kDeviceWidthMediaFeature, property));
  media_feature->set_operator(kMaximum);

  EXPECT_TRUE(media_feature->EvaluateConditionValue(ViewportSize(640, 0)));
}

TEST(MediaFeatureTest, DeviceWidthMaximumShouldEvaluateFalse) {
  scoped_refptr<LengthValue> property(new LengthValue(1280, kPixelsUnit));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kDeviceWidthMediaFeature, property));
  media_feature->set_operator(kMaximum);

  EXPECT_FALSE(media_feature->EvaluateConditionValue(ViewportSize(1920, 0)));
}

TEST(MediaFeatureTest, GridNonZeroShouldEvaluateFalse) {
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kGridMediaFeature));
  media_feature->set_operator(kNonZero);
  EXPECT_FALSE(media_feature->EvaluateConditionValue(ViewportSize(0, 0)));
}

TEST(MediaFeatureTest, GridEqualsShouldEvaluateTrue) {
  scoped_refptr<IntegerValue> property(new IntegerValue(0));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kGridMediaFeature, property));
  media_feature->set_operator(kEquals);
  EXPECT_TRUE(media_feature->EvaluateConditionValue(ViewportSize(0, 0)));
}

TEST(MediaFeatureTest, GridEqualsShouldEvaluateFalse) {
  scoped_refptr<IntegerValue> property(new IntegerValue(1));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kGridMediaFeature, property));
  media_feature->set_operator(kEquals);
  EXPECT_FALSE(media_feature->EvaluateConditionValue(ViewportSize(0, 0)));
}

TEST(MediaFeatureTest, GridMinimumShouldEvaluateTrue) {
  scoped_refptr<IntegerValue> property(new IntegerValue(0));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kGridMediaFeature, property));
  media_feature->set_operator(kMinimum);
  EXPECT_TRUE(media_feature->EvaluateConditionValue(ViewportSize(0, 0)));
}

TEST(MediaFeatureTest, GridMinimumShouldEvaluateFalse) {
  scoped_refptr<IntegerValue> property(new IntegerValue(1));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kGridMediaFeature, property));
  media_feature->set_operator(kMinimum);
  EXPECT_FALSE(media_feature->EvaluateConditionValue(ViewportSize(0, 0)));
}

TEST(MediaFeatureTest, GridMaximumShouldEvaluateTrue) {
  scoped_refptr<IntegerValue> property(new IntegerValue(0));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kGridMediaFeature, property));
  media_feature->set_operator(kMaximum);
  EXPECT_TRUE(media_feature->EvaluateConditionValue(ViewportSize(0, 0)));
}

TEST(MediaFeatureTest, HeightNonZeroShouldEvaluateTrue) {
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kHeightMediaFeature));
  media_feature->set_operator(kNonZero);

  EXPECT_TRUE(media_feature->EvaluateConditionValue(ViewportSize(0, 1080)));
}

TEST(MediaFeatureTest, HeightNonZeroShouldEvaluateFalse) {
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kHeightMediaFeature));
  media_feature->set_operator(kNonZero);

  EXPECT_FALSE(media_feature->EvaluateConditionValue(ViewportSize(0, 0)));
}

TEST(MediaFeatureTest, HeightEqualsShouldEvaluateTrue) {
  scoped_refptr<LengthValue> property(new LengthValue(1080, kPixelsUnit));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kHeightMediaFeature, property));
  media_feature->set_operator(kEquals);

  EXPECT_TRUE(media_feature->EvaluateConditionValue(ViewportSize(0, 1080)));
}

TEST(MediaFeatureTest, HeightEqualsShouldEvaluateFalse) {
  scoped_refptr<LengthValue> property(new LengthValue(1080, kPixelsUnit));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kHeightMediaFeature, property));
  media_feature->set_operator(kEquals);

  EXPECT_FALSE(media_feature->EvaluateConditionValue(ViewportSize(0, 480)));
}

TEST(MediaFeatureTest, HeightMinimumShouldEvaluateTrue) {
  scoped_refptr<LengthValue> property(new LengthValue(720, kPixelsUnit));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kHeightMediaFeature, property));
  media_feature->set_operator(kMinimum);

  EXPECT_TRUE(media_feature->EvaluateConditionValue(ViewportSize(0, 1080)));
}

TEST(MediaFeatureTest, HeightMinimumShouldEvaluateFalse) {
  scoped_refptr<LengthValue> property(new LengthValue(1080, kPixelsUnit));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kHeightMediaFeature, property));
  media_feature->set_operator(kMinimum);

  EXPECT_FALSE(media_feature->EvaluateConditionValue(ViewportSize(0, 720)));
}

TEST(MediaFeatureTest, HeightMaximumShouldEvaluateTrue) {
  scoped_refptr<LengthValue> property(new LengthValue(720, kPixelsUnit));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kHeightMediaFeature, property));
  media_feature->set_operator(kMaximum);

  EXPECT_TRUE(media_feature->EvaluateConditionValue(ViewportSize(0, 480)));
}

TEST(MediaFeatureTest, HeightMaximumShouldEvaluateFalse) {
  scoped_refptr<LengthValue> property(new LengthValue(720, kPixelsUnit));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kHeightMediaFeature, property));
  media_feature->set_operator(kMaximum);

  EXPECT_FALSE(media_feature->EvaluateConditionValue(ViewportSize(0, 1080)));
}

TEST(MediaFeatureTest, MonochromeNonZeroShouldEvaluateFalse) {
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kMonochromeMediaFeature));
  media_feature->set_operator(kNonZero);
  EXPECT_FALSE(media_feature->EvaluateConditionValue(ViewportSize(0, 0)));
}

TEST(MediaFeatureTest, MonochromeEqualsShouldEvaluateTrue) {
  scoped_refptr<IntegerValue> property(new IntegerValue(0));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kMonochromeMediaFeature, property));
  media_feature->set_operator(kEquals);
  EXPECT_TRUE(media_feature->EvaluateConditionValue(ViewportSize(0, 0)));
}

TEST(MediaFeatureTest, MonochromeEqualsShouldEvaluateFalse) {
  scoped_refptr<IntegerValue> property(new IntegerValue(8));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kMonochromeMediaFeature, property));
  media_feature->set_operator(kEquals);
  EXPECT_FALSE(media_feature->EvaluateConditionValue(ViewportSize(0, 0)));
}

TEST(MediaFeatureTest, MonochromeMinimumShouldEvaluateTrue) {
  scoped_refptr<IntegerValue> property(new IntegerValue(0));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kMonochromeMediaFeature, property));
  media_feature->set_operator(kMinimum);

  EXPECT_TRUE(media_feature->EvaluateConditionValue(ViewportSize(0, 0)));
}

TEST(MediaFeatureTest, MonochromeMinimumShouldEvaluateFalse) {
  scoped_refptr<IntegerValue> property(new IntegerValue(8));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kMonochromeMediaFeature, property));
  media_feature->set_operator(kMinimum);
  EXPECT_FALSE(media_feature->EvaluateConditionValue(ViewportSize(0, 0)));
}

TEST(MediaFeatureTest, MonochromeMaximumShouldEvaluateTrue) {
  scoped_refptr<IntegerValue> property(new IntegerValue(0));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kMonochromeMediaFeature, property));
  media_feature->set_operator(kMaximum);
  EXPECT_TRUE(media_feature->EvaluateConditionValue(ViewportSize(0, 0)));
}

TEST(MediaFeatureTest, OrientationNonZeroShouldEvaluateTrue) {
  scoped_refptr<MediaFeatureKeywordValue> property =
      MediaFeatureKeywordValue::GetPortrait();
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kOrientationMediaFeature, property));
  media_feature->set_operator(kNonZero);

  EXPECT_TRUE(media_feature->EvaluateConditionValue(ViewportSize(1920, 1080)));
}

TEST(MediaFeatureTest, OrientationEqualsShouldEvaluateTrue) {
  scoped_refptr<MediaFeatureKeywordValue> property =
      MediaFeatureKeywordValue::GetLandscape();
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kOrientationMediaFeature, property));
  media_feature->set_operator(kEquals);

  EXPECT_TRUE(media_feature->EvaluateConditionValue(ViewportSize(1920, 1080)));
}

TEST(MediaFeatureTest, OrientationEqualsShouldEvaluateFalse) {
  scoped_refptr<MediaFeatureKeywordValue> property =
      MediaFeatureKeywordValue::GetPortrait();
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kOrientationMediaFeature, property));
  media_feature->set_operator(kEquals);

  EXPECT_FALSE(media_feature->EvaluateConditionValue(ViewportSize(1920, 1080)));
}

TEST(MediaFeatureTest, ResolutionNonZeroShouldEvaluateTrue) {
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kResolutionMediaFeature));
  media_feature->set_operator(kNonZero);

  EXPECT_TRUE(media_feature->EvaluateConditionValue(ViewportSize(1920, 1080)));
}

TEST(MediaFeatureTest, ResolutionNonZeroShouldEvaluateFalse) {
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kResolutionMediaFeature));
  media_feature->set_operator(kNonZero);

  EXPECT_FALSE(media_feature->EvaluateConditionValue(
      ViewportSize(1920, 1080, 55, 0.0f)));
}

TEST(MediaFeatureTest, ResolutionEqualsDefaultShouldEvaluateTrue) {
  scoped_refptr<ResolutionValue> property(new ResolutionValue(96, kDPIUnit));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kResolutionMediaFeature, property));
  media_feature->set_operator(kEquals);

  EXPECT_TRUE(media_feature->EvaluateConditionValue(ViewportSize(1920, 1080)));
}

TEST(MediaFeatureTest, ResolutionEqualsHiDPIShouldEvaluateTrue) {
  scoped_refptr<ResolutionValue> property(new ResolutionValue(192, kDPIUnit));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kResolutionMediaFeature, property));
  media_feature->set_operator(kEquals);

  EXPECT_TRUE(media_feature->EvaluateConditionValue(
      ViewportSize(1920, 1080, 55, 2.0f)));
}

TEST(MediaFeatureTest, ResolutionEqualsShouldEvaluateFalse) {
  scoped_refptr<ResolutionValue> property(new ResolutionValue(120, kDPIUnit));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kResolutionMediaFeature, property));
  media_feature->set_operator(kEquals);

  EXPECT_FALSE(media_feature->EvaluateConditionValue(ViewportSize(1920, 1080)));
}

TEST(MediaFeatureTest, ResolutionMinimumShouldEvaluateTrue) {
  scoped_refptr<ResolutionValue> property(new ResolutionValue(20, kDPIUnit));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kResolutionMediaFeature, property));
  media_feature->set_operator(kMinimum);

  EXPECT_TRUE(media_feature->EvaluateConditionValue(ViewportSize(1920, 1080)));
}

TEST(MediaFeatureTest, ResolutionMinimumShouldEvaluateFalse) {
  scoped_refptr<ResolutionValue> property(new ResolutionValue(100, kDPIUnit));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kResolutionMediaFeature, property));
  media_feature->set_operator(kMinimum);

  EXPECT_FALSE(media_feature->EvaluateConditionValue(ViewportSize(1920, 1080)));
}

TEST(MediaFeatureTest, ResolutionMaximumShouldEvaluateTrue) {
  scoped_refptr<ResolutionValue> property(new ResolutionValue(100, kDPIUnit));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kResolutionMediaFeature, property));
  media_feature->set_operator(kMaximum);

  EXPECT_TRUE(media_feature->EvaluateConditionValue(ViewportSize(1920, 1080)));
}

TEST(MediaFeatureTest, ResolutionMaximumShouldEvaluateFalse) {
  scoped_refptr<ResolutionValue> property(new ResolutionValue(20, kDPIUnit));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kResolutionMediaFeature, property));
  media_feature->set_operator(kMaximum);

  EXPECT_FALSE(media_feature->EvaluateConditionValue(ViewportSize(1920, 1080)));
}

TEST(MediaFeatureTest, ScanNonZeroShouldEvaluateTrue) {
  scoped_refptr<MediaFeatureKeywordValue> property =
      MediaFeatureKeywordValue::GetPortrait();
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kScanMediaFeature, property));
  media_feature->set_operator(kNonZero);
  EXPECT_TRUE(media_feature->EvaluateConditionValue(ViewportSize(0, 0)));
}

TEST(MediaFeatureTest, ScanEqualsShouldEvaluateTrue) {
  scoped_refptr<MediaFeatureKeywordValue> property =
      MediaFeatureKeywordValue::GetProgressive();
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kScanMediaFeature, property));
  media_feature->set_operator(kEquals);
  EXPECT_TRUE(media_feature->EvaluateConditionValue(ViewportSize(0, 0)));
}

TEST(MediaFeatureTest, ScanEqualsShouldEvaluateFalse) {
  scoped_refptr<MediaFeatureKeywordValue> property =
      MediaFeatureKeywordValue::GetInterlace();
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kScanMediaFeature, property));
  media_feature->set_operator(kEquals);
  EXPECT_FALSE(media_feature->EvaluateConditionValue(ViewportSize(0, 0)));
}

TEST(MediaFeatureTest, WidthNonZeroShouldEvaluateTrue) {
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kWidthMediaFeature));
  media_feature->set_operator(kNonZero);

  EXPECT_TRUE(media_feature->EvaluateConditionValue(ViewportSize(1920, 0)));
}

TEST(MediaFeatureTest, WidthNonZeroShouldEvaluateFalse) {
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kWidthMediaFeature));
  media_feature->set_operator(kNonZero);

  EXPECT_FALSE(media_feature->EvaluateConditionValue(ViewportSize(0, 0)));
}

TEST(MediaFeatureTest, WidthEqualsShouldEvaluateTrue) {
  scoped_refptr<LengthValue> property(new LengthValue(1920, kPixelsUnit));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kWidthMediaFeature, property));
  media_feature->set_operator(kEquals);

  EXPECT_TRUE(media_feature->EvaluateConditionValue(ViewportSize(1920, 0)));
}

TEST(MediaFeatureTest, WidthEqualsShouldEvaluateFalse) {
  scoped_refptr<LengthValue> property(new LengthValue(1920, kPixelsUnit));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kWidthMediaFeature, property));
  media_feature->set_operator(kEquals);

  EXPECT_FALSE(media_feature->EvaluateConditionValue(ViewportSize(640, 0)));
}

TEST(MediaFeatureTest, WidthMinimumShouldEvaluateTrue) {
  scoped_refptr<LengthValue> property(new LengthValue(1280, kPixelsUnit));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kWidthMediaFeature, property));
  media_feature->set_operator(kMinimum);

  EXPECT_TRUE(media_feature->EvaluateConditionValue(ViewportSize(1920, 0)));
}

TEST(MediaFeatureTest, WidthMinimumShouldEvaluateFalse) {
  scoped_refptr<LengthValue> property(new LengthValue(1920, kPixelsUnit));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kWidthMediaFeature, property));
  media_feature->set_operator(kMinimum);

  EXPECT_FALSE(media_feature->EvaluateConditionValue(ViewportSize(1280, 0)));
}

TEST(MediaFeatureTest, WidthMaximumShouldEvaluateTrue) {
  scoped_refptr<LengthValue> property(new LengthValue(1280, kPixelsUnit));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kWidthMediaFeature, property));
  media_feature->set_operator(kMaximum);

  EXPECT_TRUE(media_feature->EvaluateConditionValue(ViewportSize(640, 0)));
}

TEST(MediaFeatureTest, WidthMaximumShouldEvaluateFalse) {
  scoped_refptr<LengthValue> property(new LengthValue(1280, kPixelsUnit));
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kWidthMediaFeature, property));
  media_feature->set_operator(kMaximum);

  EXPECT_FALSE(media_feature->EvaluateConditionValue(ViewportSize(1920, 0)));
}

}  // namespace
}  // namespace cssom
}  // namespace cobalt
