// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "third_party/blink/renderer/modules/cobalt/h5vcc_experiments/experiments_utils.h"

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_boolean_long_string.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_experiment_configuration.h"

namespace blink {

TEST(ExperimentsUtilsTest, AllFieldsPresent) {
  auto* config = MakeGarbageCollected<ExperimentConfiguration>();
  Vector<std::pair<String, bool>> features_vector;
  features_vector.push_back(std::make_pair(String::FromUTF8("FeatureA"), true));
  features_vector.push_back(
      std::make_pair(String::FromUTF8("FeatureB"), false));
  config->setFeatures(features_vector);

  HeapVector<std::pair<String, Member<V8UnionBooleanOrLongOrString>>>
      feature_params_vector;

  const String str = "value1";
  Member<V8UnionBooleanOrLongOrString> union_str =
      MakeGarbageCollected<V8UnionBooleanOrLongOrString>(str);
  Member<V8UnionBooleanOrLongOrString> union_long =
      MakeGarbageCollected<V8UnionBooleanOrLongOrString>(123);
  Member<V8UnionBooleanOrLongOrString> union_bool_true =
      MakeGarbageCollected<V8UnionBooleanOrLongOrString>(true);
  Member<V8UnionBooleanOrLongOrString> union_bool_false =
      MakeGarbageCollected<V8UnionBooleanOrLongOrString>(false);
  feature_params_vector.push_back(
      std::make_pair(String::FromUTF8("ParamString"), union_str));
  feature_params_vector.push_back(
      std::make_pair(String::FromUTF8("ParamLong"), union_long));
  feature_params_vector.push_back(
      std::make_pair(String::FromUTF8("ParamBoolTrue"), union_bool_true));
  feature_params_vector.push_back(
      std::make_pair(String::FromUTF8("ParamBoolFalse"), union_bool_false));
  config->setFeatureParams(feature_params_vector);

<<<<<<< HEAD
  Vector<uint32_t> exp_ids_vector;
  exp_ids_vector.push_back(1001);
  exp_ids_vector.push_back(1002);
  config->setExperimentIds(exp_ids_vector);
=======
  config->setActiveExperimentConfigData(String::FromUTF8("active_config_data"));
  config->setLatestExperimentConfigHashData(
      String::FromUTF8("latest_hash_data"));
>>>>>>> cec25ce38c4 (Switch std usages to base utilities. (#7763))

  std::optional<base::Value::Dict> result = ParseConfigToDictionary(config);

  ASSERT_TRUE(result.has_value());
  const base::Value::Dict& dict = result.value();

  const base::Value::Dict* features_dict =
      dict.FindDict(cobalt::kExperimentConfigFeatures);
  ASSERT_NE(nullptr, features_dict);
  EXPECT_EQ(true, features_dict->FindBool("FeatureA").value_or(false));
  EXPECT_EQ(false, features_dict->FindBool("FeatureB").value_or(true));

  const base::Value::Dict* feature_params_dict =
      dict.FindDict(cobalt::kExperimentConfigFeatureParams);
  ASSERT_NE(nullptr, feature_params_dict);
  EXPECT_EQ("value1", *feature_params_dict->FindString("ParamString"));
  EXPECT_EQ("123", *feature_params_dict->FindString("ParamLong"));
<<<<<<< HEAD
=======
  EXPECT_EQ("1.23", *feature_params_dict->FindString("ParamDouble"));
>>>>>>> cec25ce38c4 (Switch std usages to base utilities. (#7763))
  EXPECT_EQ("true", *feature_params_dict->FindString("ParamBoolTrue"));
  EXPECT_EQ("false", *feature_params_dict->FindString("ParamBoolFalse"));

  const base::Value::List* exp_ids_list =
      dict.FindList(cobalt::kExperimentConfigExpIds);
  ASSERT_NE(nullptr, exp_ids_list);
  ASSERT_EQ(2u, exp_ids_list->size());
  EXPECT_EQ(1001, (*exp_ids_list)[0].GetInt());
  EXPECT_EQ(1002, (*exp_ids_list)[1].GetInt());
}

TEST(ExperimentsUtilsTest, ParseConfigToDictionary_MissingFeatures) {
  auto* config = MakeGarbageCollected<ExperimentConfiguration>();
  // setFeatures NOT called in this test.
  config->setFeatureParams(
      HeapVector<std::pair<String, Member<V8UnionBooleanOrLongOrString>>>());
  config->setExperimentIds(Vector<uint32_t>());

  std::optional<base::Value::Dict> result = ParseConfigToDictionary(config);
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result.value()
                  .Find(cobalt::kExperimentConfigFeatures)
                  ->GetIfDict()
                  ->empty());
}

TEST(ExperimentsUtilsTest, ParseConfigToDictionary_MissingFeatureParams) {
  auto* config = MakeGarbageCollected<ExperimentConfiguration>();
  config->setFeatures(Vector<std::pair<String, bool>>());
  // setFeatureParams NOT called in this test.
  config->setExperimentIds(Vector<uint32_t>());

  std::optional<base::Value::Dict> result = ParseConfigToDictionary(config);
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result.value()
                  .Find(cobalt::kExperimentConfigFeatureParams)
                  ->GetIfDict()
                  ->empty());
}

TEST(ExperimentsUtilsTest, ParseConfigToDictionary_MissingExperimentIds) {
  auto* config = MakeGarbageCollected<ExperimentConfiguration>();
  config->setFeatures(Vector<std::pair<String, bool>>());
  config->setFeatureParams(
      HeapVector<std::pair<String, Member<V8UnionBooleanOrLongOrString>>>());
  // setExperimentIds NOT called in this test.

  std::optional<base::Value::Dict> result = ParseConfigToDictionary(config);
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result.value()
                  .Find(cobalt::kExperimentConfigExpIds)
                  ->GetIfList()
                  ->empty());
}

TEST(ExperimentsUtilsTest, ParseConfigToDictionary_AllFieldsPresentButEmpty) {
  auto* config = MakeGarbageCollected<ExperimentConfiguration>();

  ASSERT_TRUE(true);
  config->setFeatures(Vector<std::pair<String, bool>>());
  config->setFeatureParams(
      HeapVector<std::pair<String, Member<V8UnionBooleanOrLongOrString>>>());
  config->setExperimentIds(Vector<uint32_t>());

  std::optional<base::Value::Dict> result = ParseConfigToDictionary(config);
  ASSERT_TRUE(result.has_value());
  const base::Value::Dict& dict = result.value();

  const base::Value::Dict* features_dict =
      dict.FindDict(cobalt::kExperimentConfigFeatures);
  ASSERT_NE(nullptr, features_dict);
  EXPECT_TRUE(features_dict->empty());

  const base::Value::Dict* feature_params_dict =
      dict.FindDict(cobalt::kExperimentConfigFeatureParams);
  ASSERT_NE(nullptr, feature_params_dict);
  EXPECT_TRUE(feature_params_dict->empty());

  const base::Value::List* exp_ids_list =
      dict.FindList(cobalt::kExperimentConfigExpIds);
  ASSERT_NE(nullptr, exp_ids_list);
  EXPECT_TRUE(exp_ids_list->empty());
}

TEST(ExperimentsUtilsTest, IsTrueDoubleTest) {
  EXPECT_TRUE(IsTrueDouble(1.1));
  EXPECT_FALSE(IsTrueDouble(1.0));
  EXPECT_TRUE(IsTrueDouble(-1.1));
  EXPECT_FALSE(IsTrueDouble(-1.0));
  EXPECT_FALSE(IsTrueDouble(0.0));
  double large_double =
      static_cast<double>(std::numeric_limits<int>::max()) + 10.0;
  EXPECT_TRUE(IsTrueDouble(large_double));
  EXPECT_FALSE(
      IsTrueDouble(static_cast<double>(std::numeric_limits<int>::max())));
  EXPECT_FALSE(
      IsTrueDouble(static_cast<double>(std::numeric_limits<int>::min())));
}

TEST(ExperimentsUtilsTest, ParseConfigToDictionaryDoubleConversion) {
  auto* config = MakeGarbageCollected<ExperimentConfiguration>();
  config->setFeatures(Vector<std::pair<String, bool>>());
  config->setActiveExperimentConfigData(String::FromUTF8(""));
  config->setLatestExperimentConfigHashData(String::FromUTF8(""));

  HeapVector<std::pair<String, Member<V8Union>>> feature_params_vector;

  // A double with a fractional part.
  feature_params_vector.push_back(
      std::make_pair(String::FromUTF8("DoubleWithFraction"),
                     MakeGarbageCollected<V8Union>(123.456)));
  // A double that is a whole number and fits within a 32-bit integer.
  feature_params_vector.push_back(std::make_pair(
      String::FromUTF8("DoubleAsInt"), MakeGarbageCollected<V8Union>(123.0)));
  // A double that is a whole number but too large for a 32-bit integer.
  double large_double =
      static_cast<double>(std::numeric_limits<int>::max()) + 10.0;
  feature_params_vector.push_back(
      std::make_pair(String::FromUTF8("DoubleTooLargeForInt"),
                     MakeGarbageCollected<V8Union>(large_double)));

  config->setFeatureParams(feature_params_vector);

  std::optional<base::Value::Dict> result = ParseConfigToDictionary(config);
  ASSERT_TRUE(result.has_value());
  const base::Value::Dict* params =
      result->FindDict(cobalt::kExperimentConfigFeatureParams);
  ASSERT_NE(nullptr, params);

  EXPECT_EQ(base::NumberToString(123.456),
            *params->FindString("DoubleWithFraction"));
  EXPECT_EQ(base::NumberToString(123), *params->FindString("DoubleAsInt"));
  EXPECT_EQ(base::NumberToString(large_double),
            *params->FindString("DoubleTooLargeForInt"));
}

TEST(ExperimentsUtilsTest, ParseSettingsToDictionaryDoubleConversion) {
  HeapVector<std::pair<WTF::String, Member<V8Union>>> settings;

  // A double with a fractional part.
  settings.push_back(std::make_pair("DoubleWithFraction",
                                    MakeGarbageCollected<V8Union>(123.456)));
  // A double that is a whole number and fits within a 32-bit integer.
  settings.push_back(
      std::make_pair("DoubleAsInt", MakeGarbageCollected<V8Union>(123.0)));
  // A double that is a whole number but too large for a 32-bit integer.
  double large_double =
      static_cast<double>(std::numeric_limits<int>::max()) + 10.0;
  settings.push_back(std::make_pair(
      "DoubleTooLargeForInt", MakeGarbageCollected<V8Union>(large_double)));

  auto result = ParseSettingsToDictionary(settings);
  EXPECT_TRUE(result.has_value());

  EXPECT_EQ(123.456, result->FindDouble("DoubleWithFraction").value());
  EXPECT_EQ(123, result->FindInt("DoubleAsInt").value());
  EXPECT_EQ(large_double, result->FindDouble("DoubleTooLargeForInt").value());
}

}  // namespace blink
