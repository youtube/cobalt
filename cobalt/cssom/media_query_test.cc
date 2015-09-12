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

#include "cobalt/cssom/media_query.h"

#include "base/memory/ref_counted.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/media_feature.h"
#include "cobalt/cssom/media_feature_keyword_value.h"
#include "cobalt/cssom/media_feature_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace cssom {

TEST(MediaQueryTest, EvaluateEmptyQuery) {
  scoped_refptr<MediaQuery> media_query(new MediaQuery());
  EXPECT_TRUE(media_query->EvaluateConditionValue(NULL, NULL));
}

TEST(MediaQueryTest, EvaluateMediaTypeFalse) {
  scoped_refptr<MediaQuery> media_query(new MediaQuery(false));
  EXPECT_FALSE(media_query->EvaluateConditionValue(NULL, NULL));
}

TEST(MediaQueryTest, EvaluateMediaTypeTrue) {
  scoped_refptr<MediaQuery> media_query(new MediaQuery(true));
  EXPECT_TRUE(media_query->EvaluateConditionValue(NULL, NULL));
}

TEST(MediaQueryTest, EvaluateMediaTypeFalseAndMediaFeatureTrue) {
  scoped_refptr<MediaFeatureKeywordValue> property =
      MediaFeatureKeywordValue::GetLandscape();
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kOrientationMediaFeature, property));
  media_feature->set_operator(kEquals);
  scoped_refptr<LengthValue> width(new LengthValue(1920, kPixelsUnit));
  scoped_refptr<LengthValue> height(new LengthValue(1080, kPixelsUnit));
  EXPECT_TRUE(media_feature->EvaluateConditionValue(width, height));

  scoped_ptr<MediaFeatures> media_features(new MediaFeatures);
  media_features->push_back(media_feature);
  scoped_refptr<MediaQuery> media_query(
      new MediaQuery(false, media_features.Pass()));
  EXPECT_FALSE(media_query->EvaluateConditionValue(width, height));
}

TEST(MediaQueryTest, EvaluateMediaTypeFalseAndMediaFeatureFalse) {
  scoped_refptr<MediaFeatureKeywordValue> property =
      MediaFeatureKeywordValue::GetPortrait();
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kOrientationMediaFeature, property));
  media_feature->set_operator(kEquals);
  scoped_refptr<LengthValue> width(new LengthValue(1920, kPixelsUnit));
  scoped_refptr<LengthValue> height(new LengthValue(1080, kPixelsUnit));
  EXPECT_FALSE(media_feature->EvaluateConditionValue(width, height));

  scoped_ptr<MediaFeatures> media_features(new MediaFeatures);
  media_features->push_back(media_feature);
  scoped_refptr<MediaQuery> media_query(
      new MediaQuery(false, media_features.Pass()));
  EXPECT_FALSE(media_query->EvaluateConditionValue(width, height));
}

TEST(MediaQueryTest, EvaluateMediaTypeTrueAndMediaFeatureTrue) {
  scoped_refptr<MediaFeatureKeywordValue> property =
      MediaFeatureKeywordValue::GetLandscape();
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kOrientationMediaFeature, property));
  media_feature->set_operator(kEquals);
  scoped_refptr<LengthValue> width(new LengthValue(1920, kPixelsUnit));
  scoped_refptr<LengthValue> height(new LengthValue(1080, kPixelsUnit));
  EXPECT_TRUE(media_feature->EvaluateConditionValue(width, height));

  scoped_ptr<MediaFeatures> media_features(new MediaFeatures);
  media_features->push_back(media_feature);
  scoped_refptr<MediaQuery> media_query(
      new MediaQuery(true, media_features.Pass()));
  EXPECT_TRUE(media_query->EvaluateConditionValue(width, height));
}

TEST(MediaQueryTest, EvaluateMediaTypeTrueAndMediaFeatureFalse) {
  scoped_refptr<MediaFeatureKeywordValue> property =
      MediaFeatureKeywordValue::GetPortrait();
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kOrientationMediaFeature, property));
  media_feature->set_operator(kEquals);
  scoped_refptr<LengthValue> width(new LengthValue(1920, kPixelsUnit));
  scoped_refptr<LengthValue> height(new LengthValue(1080, kPixelsUnit));
  EXPECT_FALSE(media_feature->EvaluateConditionValue(width, height));

  scoped_ptr<MediaFeatures> media_features(new MediaFeatures);
  media_features->push_back(media_feature);
  scoped_refptr<MediaQuery> media_query(
      new MediaQuery(true, media_features.Pass()));

  EXPECT_FALSE(media_query->EvaluateConditionValue(width, height));
}

TEST(MediaQueryTest, EvaluateMediaTypeTrueAndFeaturesFalseAndFalse) {
  scoped_refptr<MediaFeatureKeywordValue> property_1 =
      MediaFeatureKeywordValue::GetPortrait();
  scoped_refptr<MediaFeature> media_feature_1(
      new MediaFeature(kOrientationMediaFeature, property_1));
  media_feature_1->set_operator(kEquals);
  scoped_refptr<LengthValue> width(new LengthValue(1920, kPixelsUnit));
  scoped_refptr<LengthValue> height(new LengthValue(1080, kPixelsUnit));
  EXPECT_FALSE(media_feature_1->EvaluateConditionValue(width, height));

  scoped_refptr<MediaFeatureKeywordValue> property_2 =
      MediaFeatureKeywordValue::GetInterlace();
  scoped_refptr<MediaFeature> media_feature_2(
      new MediaFeature(kScanMediaFeature, property_2));
  media_feature_2->set_operator(kEquals);
  EXPECT_FALSE(media_feature_2->EvaluateConditionValue(width, height));

  scoped_ptr<MediaFeatures> media_features(new MediaFeatures);
  media_features->push_back(media_feature_1);
  media_features->push_back(media_feature_2);
  scoped_refptr<MediaQuery> media_query(
      new MediaQuery(true, media_features.Pass()));

  EXPECT_FALSE(media_query->EvaluateConditionValue(width, height));
}

TEST(MediaQueryTest, EvaluateMediaTypeTrueAndFeaturesFalseAndTrue) {
  scoped_refptr<MediaFeatureKeywordValue> property_1 =
      MediaFeatureKeywordValue::GetPortrait();
  scoped_refptr<MediaFeature> media_feature_1(
      new MediaFeature(kOrientationMediaFeature, property_1));
  media_feature_1->set_operator(kEquals);
  scoped_refptr<LengthValue> width(new LengthValue(1920, kPixelsUnit));
  scoped_refptr<LengthValue> height(new LengthValue(1080, kPixelsUnit));
  EXPECT_FALSE(media_feature_1->EvaluateConditionValue(width, height));

  scoped_refptr<MediaFeatureKeywordValue> property_2 =
      MediaFeatureKeywordValue::GetProgressive();
  scoped_refptr<MediaFeature> media_feature_2(
      new MediaFeature(kScanMediaFeature, property_2));
  media_feature_2->set_operator(kEquals);
  EXPECT_TRUE(media_feature_2->EvaluateConditionValue(width, height));

  scoped_ptr<MediaFeatures> media_features(new MediaFeatures);
  media_features->push_back(media_feature_1);
  media_features->push_back(media_feature_2);
  scoped_refptr<MediaQuery> media_query(
      new MediaQuery(true, media_features.Pass()));
  EXPECT_FALSE(media_query->EvaluateConditionValue(width, height));
}

TEST(MediaQueryTest, EvaluateMediaTypeTrueAndFeaturesTrueAndFalse) {
  scoped_refptr<MediaFeatureKeywordValue> property_1 =
      MediaFeatureKeywordValue::GetLandscape();
  scoped_refptr<MediaFeature> media_feature_1(
      new MediaFeature(kOrientationMediaFeature, property_1));
  media_feature_1->set_operator(kEquals);
  scoped_refptr<LengthValue> width(new LengthValue(1920, kPixelsUnit));
  scoped_refptr<LengthValue> height(new LengthValue(1080, kPixelsUnit));
  EXPECT_TRUE(media_feature_1->EvaluateConditionValue(width, height));

  scoped_refptr<MediaFeatureKeywordValue> property_2 =
      MediaFeatureKeywordValue::GetInterlace();
  scoped_refptr<MediaFeature> media_feature_2(
      new MediaFeature(kScanMediaFeature, property_2));
  media_feature_2->set_operator(kEquals);
  EXPECT_FALSE(media_feature_2->EvaluateConditionValue(width, height));

  scoped_ptr<MediaFeatures> media_features(new MediaFeatures);
  media_features->push_back(media_feature_1);
  media_features->push_back(media_feature_2);
  scoped_refptr<MediaQuery> media_query(
      new MediaQuery(true, media_features.Pass()));
  EXPECT_FALSE(media_query->EvaluateConditionValue(width, height));
}

TEST(MediaQueryTest, EvaluateMediaTypeTrueAndFeaturesTrueAndTrue) {
  scoped_refptr<MediaFeatureKeywordValue> property_1 =
      MediaFeatureKeywordValue::GetLandscape();
  scoped_refptr<MediaFeature> media_feature_1(
      new MediaFeature(kOrientationMediaFeature, property_1));
  media_feature_1->set_operator(kEquals);
  scoped_refptr<LengthValue> width(new LengthValue(1920, kPixelsUnit));
  scoped_refptr<LengthValue> height(new LengthValue(1080, kPixelsUnit));
  EXPECT_TRUE(media_feature_1->EvaluateConditionValue(width, height));

  scoped_refptr<MediaFeatureKeywordValue> property_2 =
      MediaFeatureKeywordValue::GetProgressive();
  scoped_refptr<MediaFeature> media_feature_2(
      new MediaFeature(kScanMediaFeature, property_2));
  media_feature_2->set_operator(kEquals);
  EXPECT_TRUE(media_feature_2->EvaluateConditionValue(width, height));

  scoped_ptr<MediaFeatures> media_features(new MediaFeatures);
  media_features->push_back(media_feature_1);
  media_features->push_back(media_feature_2);
  scoped_refptr<MediaQuery> media_query(
      new MediaQuery(true, media_features.Pass()));
  EXPECT_TRUE(media_query->EvaluateConditionValue(width, height));
}

}  // namespace cssom
}  // namespace cobalt
