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

#include "cobalt/cssom/media_query.h"

#include "base/memory/ref_counted.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/media_feature.h"
#include "cobalt/cssom/media_feature_keyword_value.h"
#include "cobalt/cssom/media_feature_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace cssom {

namespace {

TEST(MediaQueryTest, EvaluateEmptyQuery) {
  scoped_refptr<MediaQuery> media_query(new MediaQuery());
  EXPECT_TRUE(media_query->EvaluateConditionValue(ViewportSize(0, 0)));
}

TEST(MediaQueryTest, EvaluateMediaTypeFalse) {
  scoped_refptr<MediaQuery> media_query(new MediaQuery(false));
  EXPECT_FALSE(media_query->EvaluateConditionValue(ViewportSize(0, 0)));
}

TEST(MediaQueryTest, EvaluateMediaTypeTrue) {
  scoped_refptr<MediaQuery> media_query(new MediaQuery(true));
  EXPECT_TRUE(media_query->EvaluateConditionValue(ViewportSize(0, 0)));
}

TEST(MediaQueryTest, EvaluateMediaTypeFalseAndMediaFeatureTrue) {
  scoped_refptr<MediaFeatureKeywordValue> property =
      MediaFeatureKeywordValue::GetLandscape();
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kOrientationMediaFeature, property));
  media_feature->set_operator(kEquals);
  EXPECT_TRUE(media_feature->EvaluateConditionValue(ViewportSize(1920, 1080)));

  std::unique_ptr<MediaFeatures> media_features(new MediaFeatures);
  media_features->push_back(media_feature);
  scoped_refptr<MediaQuery> media_query(
      new MediaQuery(false, std::move(media_features)));
  EXPECT_FALSE(media_query->EvaluateConditionValue(ViewportSize(1920, 1080)));
}

TEST(MediaQueryTest, EvaluateMediaTypeFalseAndMediaFeatureFalse) {
  scoped_refptr<MediaFeatureKeywordValue> property =
      MediaFeatureKeywordValue::GetPortrait();
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kOrientationMediaFeature, property));
  media_feature->set_operator(kEquals);
  EXPECT_FALSE(media_feature->EvaluateConditionValue(ViewportSize(1920, 108)));

  std::unique_ptr<MediaFeatures> media_features(new MediaFeatures);
  media_features->push_back(media_feature);
  scoped_refptr<MediaQuery> media_query(
      new MediaQuery(false, std::move(media_features)));
  EXPECT_FALSE(media_query->EvaluateConditionValue(ViewportSize(1920, 1080)));
}

TEST(MediaQueryTest, EvaluateMediaTypeTrueAndMediaFeatureTrue) {
  scoped_refptr<MediaFeatureKeywordValue> property =
      MediaFeatureKeywordValue::GetLandscape();
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kOrientationMediaFeature, property));
  media_feature->set_operator(kEquals);
  EXPECT_TRUE(media_feature->EvaluateConditionValue(ViewportSize(1920, 1080)));

  std::unique_ptr<MediaFeatures> media_features(new MediaFeatures);
  media_features->push_back(media_feature);
  scoped_refptr<MediaQuery> media_query(
      new MediaQuery(true, std::move(media_features)));
  EXPECT_TRUE(media_query->EvaluateConditionValue(ViewportSize(1920, 1080)));
}

TEST(MediaQueryTest, EvaluateMediaTypeTrueAndMediaFeatureFalse) {
  scoped_refptr<MediaFeatureKeywordValue> property =
      MediaFeatureKeywordValue::GetPortrait();
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kOrientationMediaFeature, property));
  media_feature->set_operator(kEquals);
  EXPECT_FALSE(media_feature->EvaluateConditionValue(ViewportSize(1920, 1080)));

  std::unique_ptr<MediaFeatures> media_features(new MediaFeatures);
  media_features->push_back(media_feature);
  scoped_refptr<MediaQuery> media_query(
      new MediaQuery(true, std::move(media_features)));

  EXPECT_FALSE(media_query->EvaluateConditionValue(ViewportSize(1920, 1080)));
}

TEST(MediaQueryTest, EvaluateMediaTypeTrueAndFeaturesFalseAndFalse) {
  scoped_refptr<MediaFeatureKeywordValue> property_1 =
      MediaFeatureKeywordValue::GetPortrait();
  scoped_refptr<MediaFeature> media_feature_1(
      new MediaFeature(kOrientationMediaFeature, property_1));
  media_feature_1->set_operator(kEquals);
  EXPECT_FALSE(
      media_feature_1->EvaluateConditionValue(ViewportSize(1920, 1080)));

  scoped_refptr<MediaFeatureKeywordValue> property_2 =
      MediaFeatureKeywordValue::GetInterlace();
  scoped_refptr<MediaFeature> media_feature_2(
      new MediaFeature(kScanMediaFeature, property_2));
  media_feature_2->set_operator(kEquals);
  EXPECT_FALSE(
      media_feature_2->EvaluateConditionValue(ViewportSize(1920, 1080)));

  std::unique_ptr<MediaFeatures> media_features(new MediaFeatures);
  media_features->push_back(media_feature_1);
  media_features->push_back(media_feature_2);
  scoped_refptr<MediaQuery> media_query(
      new MediaQuery(true, std::move(media_features)));

  EXPECT_FALSE(media_query->EvaluateConditionValue(ViewportSize(1920, 1080)));
}

TEST(MediaQueryTest, EvaluateMediaTypeTrueAndFeaturesFalseAndTrue) {
  scoped_refptr<MediaFeatureKeywordValue> property_1 =
      MediaFeatureKeywordValue::GetPortrait();
  scoped_refptr<MediaFeature> media_feature_1(
      new MediaFeature(kOrientationMediaFeature, property_1));
  media_feature_1->set_operator(kEquals);
  EXPECT_FALSE(
      media_feature_1->EvaluateConditionValue(ViewportSize(1920, 1080)));

  scoped_refptr<MediaFeatureKeywordValue> property_2 =
      MediaFeatureKeywordValue::GetProgressive();
  scoped_refptr<MediaFeature> media_feature_2(
      new MediaFeature(kScanMediaFeature, property_2));
  media_feature_2->set_operator(kEquals);
  EXPECT_TRUE(
      media_feature_2->EvaluateConditionValue(ViewportSize(1920, 1080)));

  std::unique_ptr<MediaFeatures> media_features(new MediaFeatures);
  media_features->push_back(media_feature_1);
  media_features->push_back(media_feature_2);
  scoped_refptr<MediaQuery> media_query(
      new MediaQuery(true, std::move(media_features)));
  EXPECT_FALSE(media_query->EvaluateConditionValue(ViewportSize(1920, 1080)));
}

TEST(MediaQueryTest, EvaluateMediaTypeTrueAndFeaturesTrueAndFalse) {
  scoped_refptr<MediaFeatureKeywordValue> property_1 =
      MediaFeatureKeywordValue::GetLandscape();
  scoped_refptr<MediaFeature> media_feature_1(
      new MediaFeature(kOrientationMediaFeature, property_1));
  media_feature_1->set_operator(kEquals);
  EXPECT_TRUE(
      media_feature_1->EvaluateConditionValue(ViewportSize(1920, 1080)));

  scoped_refptr<MediaFeatureKeywordValue> property_2 =
      MediaFeatureKeywordValue::GetInterlace();
  scoped_refptr<MediaFeature> media_feature_2(
      new MediaFeature(kScanMediaFeature, property_2));
  media_feature_2->set_operator(kEquals);
  EXPECT_FALSE(
      media_feature_2->EvaluateConditionValue(ViewportSize(1920, 1080)));

  std::unique_ptr<MediaFeatures> media_features(new MediaFeatures);
  media_features->push_back(media_feature_1);
  media_features->push_back(media_feature_2);
  scoped_refptr<MediaQuery> media_query(
      new MediaQuery(true, std::move(media_features)));
  EXPECT_FALSE(media_query->EvaluateConditionValue(ViewportSize(1920, 1080)));
}

TEST(MediaQueryTest, EvaluateMediaTypeTrueAndFeaturesTrueAndTrue) {
  scoped_refptr<MediaFeatureKeywordValue> property_1 =
      MediaFeatureKeywordValue::GetLandscape();
  scoped_refptr<MediaFeature> media_feature_1(
      new MediaFeature(kOrientationMediaFeature, property_1));
  media_feature_1->set_operator(kEquals);
  EXPECT_TRUE(
      media_feature_1->EvaluateConditionValue(ViewportSize(1920, 1080)));

  scoped_refptr<MediaFeatureKeywordValue> property_2 =
      MediaFeatureKeywordValue::GetProgressive();
  scoped_refptr<MediaFeature> media_feature_2(
      new MediaFeature(kScanMediaFeature, property_2));
  media_feature_2->set_operator(kEquals);
  EXPECT_TRUE(
      media_feature_2->EvaluateConditionValue(ViewportSize(1920, 1080)));

  std::unique_ptr<MediaFeatures> media_features(new MediaFeatures);
  media_features->push_back(media_feature_1);
  media_features->push_back(media_feature_2);
  scoped_refptr<MediaQuery> media_query(
      new MediaQuery(true, std::move(media_features)));
  EXPECT_TRUE(media_query->EvaluateConditionValue(ViewportSize(1920, 1080)));
}

}  // namespace
}  // namespace cssom
}  // namespace cobalt
