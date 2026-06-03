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
#include "third_party/blink/renderer/bindings/core/v8/v8_union_boolean_double_long_string.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_experiment_configuration.h"

namespace blink {

using V8Union = V8UnionBooleanOrDoubleOrLongOrString;

TEST(ExperimentsUtilsTest, AllFieldsPresent) {
  auto* config = MakeGarbageCollected<ExperimentConfiguration>();
  Vector<std::pair<String, bool>> features_vector;
  features_vector.push_back(std::make_pair(String::FromUTF8("FeatureA"), true));
  features_vector.push_back(
      std::make_pair(String::FromUTF8("FeatureB"), false));
  config->setFeatures(features_vector);

  HeapVector<std::pair<String, Member<V8Union>>> feature_params_vector;

  const String str = "value1";
  Member<V8Union> union_str = MakeGarbageCollected<V8Union>(str);
  Member<V8Union> union_long = MakeGarbageCollected<V8Union>(123);
  Member<V8Union> union_double = MakeGarbageCollected<V8Union>(1.23);
  Member<V8Union> union_bool_true = MakeGarbageCollected<V8Union>(true);
  Member<V8Union> union_bool_false = MakeGarbageCollected<V8Union>(false);
  feature_params_vector.push_back(
      std::make_pair(String::FromUTF8("ParamString"), union_str));
  feature_params_vector.push_back(
      std::make_pair(String::FromUTF8("ParamLong"), union_long));
  feature_params_vector.push_back(
      std::make_pair(String::FromUTF8("ParamDouble"), union_double));
  feature_params_vector.push_back(
      std::make_pair(String::FromUTF8("ParamBoolTrue"), union_bool_true));
  feature_params_vector.push_back(
      std::make_pair(String::FromUTF8("ParamBoolFalse"), union_bool_false));
  config->setFeatureParams(feature_params_vector);

  config->setActiveExperimentConfigData(String::FromUTF8("active_config_data"));
  config->setLatestExperimentConfigHashData(
      String::FromUTF8("latest_hash_data"));

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
  EXPECT_EQ("1.23", *feature_params_dict->FindString("ParamDouble"));
  EXPECT_EQ("true", *feature_params_dict->FindString("ParamBoolTrue"));
  EXPECT_EQ("false", *feature_params_dict->FindString("ParamBoolFalse"));

  const std::string active_config_data =
      *dict.FindString(cobalt::kExperimentConfigActiveConfigData);
  EXPECT_EQ("active_config_data", active_config_data);

  const std::string latest_hash_data =
      *dict.FindString(cobalt::kLatestConfigHash);
  EXPECT_EQ("latest_hash_data", latest_hash_data);
}

TEST(ExperimentsUtilsTest, ParseConfigToDictionary_MissingFeatures) {
  auto* config = MakeGarbageCollected<ExperimentConfiguration>();
  // setFeatures NOT called in this test.
  config->setFeatureParams(HeapVector<std::pair<String, Member<V8Union>>>());
  config->setActiveExperimentConfigData(String::FromUTF8(""));
  config->setLatestExperimentConfigHashData(String::FromUTF8(""));

  std::optional<base::Value::Dict> result = ParseConfigToDictionary(config);
  ASSERT_FALSE(result.has_value());
}

TEST(ExperimentsUtilsTest, ParseConfigToDictionary_MissingFeatureParams) {
  auto* config = MakeGarbageCollected<ExperimentConfiguration>();
  config->setFeatures(Vector<std::pair<String, bool>>());
  // setFeatureParams NOT called in this test.
  config->setActiveExperimentConfigData(String::FromUTF8(""));
  config->setLatestExperimentConfigHashData(String::FromUTF8(""));

  std::optional<base::Value::Dict> result = ParseConfigToDictionary(config);
  ASSERT_FALSE(result.has_value());
}

TEST(ExperimentsUtilsTest,
     ParseConfigToDictionary_MissingActiveExperimentConfigData) {
  auto* config = MakeGarbageCollected<ExperimentConfiguration>();
  config->setFeatures(Vector<std::pair<String, bool>>());
  config->setFeatureParams(HeapVector<std::pair<String, Member<V8Union>>>());
  config->setLatestExperimentConfigHashData(String::FromUTF8(""));
  // setActiveExperimentConfigData NOT called in this test.

  std::optional<base::Value::Dict> result = ParseConfigToDictionary(config);
  ASSERT_FALSE(result.has_value());
}

TEST(ExperimentsUtilsTest,
     ParseConfigToDictionary_MissingLatestExperimentConfigHashData) {
  auto* config = MakeGarbageCollected<ExperimentConfiguration>();
  config->setFeatures(Vector<std::pair<String, bool>>());
  config->setFeatureParams(HeapVector<std::pair<String, Member<V8Union>>>());
  config->setActiveExperimentConfigData(String::FromUTF8(""));
  // setLatestExperimentConfigHashData NOT called in this test.

  std::optional<base::Value::Dict> result = ParseConfigToDictionary(config);
  ASSERT_FALSE(result.has_value());
}

TEST(ExperimentsUtilsTest, ParseConfigToDictionary_AllFieldsPresentButEmpty) {
  auto* config = MakeGarbageCollected<ExperimentConfiguration>();

  ASSERT_TRUE(true);
  config->setFeatures(Vector<std::pair<String, bool>>());
  config->setFeatureParams(HeapVector<std::pair<String, Member<V8Union>>>());
  config->setActiveExperimentConfigData(String::FromUTF8(""));
  config->setLatestExperimentConfigHashData(String::FromUTF8(""));

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

  const std::string active_config_data =
      *dict.FindString(cobalt::kExperimentConfigActiveConfigData);
  EXPECT_EQ("", active_config_data);

  const std::string latest_hash_data =
      *dict.FindString(cobalt::kLatestConfigHash);
  EXPECT_EQ("", latest_hash_data);
}

TEST(ExperimentsUtilsTest, ParseSettingsToDictionaryEmpty) {
  HeapVector<std::pair<WTF::String, Member<V8Union>>> settings;
  auto result = ParseSettingsToDictionary(settings);
  EXPECT_TRUE(result.has_value());
  EXPECT_TRUE(result->empty());
}

TEST(ExperimentsUtilsTest, ParseSettingsToDictionaryString) {
  HeapVector<std::pair<WTF::String, Member<V8Union>>> settings;
  settings.push_back(
      std::make_pair("key", MakeGarbageCollected<V8Union>(String("value"))));
  auto result = ParseSettingsToDictionary(settings);
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(1u, result->size());
  const std::string* value = result->FindString("key");
  EXPECT_TRUE(value);
  EXPECT_EQ("value", *value);
}

TEST(ExperimentsUtilsTest, ParseSettingsToDictionaryLong) {
  HeapVector<std::pair<WTF::String, Member<V8Union>>> settings;
  settings.push_back(std::make_pair("key", MakeGarbageCollected<V8Union>(123)));
  auto result = ParseSettingsToDictionary(settings);
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(1u, result->size());
  absl::optional<int> value = result->FindInt("key");
  EXPECT_TRUE(value.has_value());
  EXPECT_EQ(123, value.value());
}

TEST(ExperimentsUtilsTest, ParseSettingsToDictionaryDouble) {
  HeapVector<std::pair<WTF::String, Member<V8Union>>> settings;
  settings.push_back(
      std::make_pair("key", MakeGarbageCollected<V8Union>(123.456)));
  auto result = ParseSettingsToDictionary(settings);
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(1u, result->size());
  absl::optional<double> value = result->FindDouble("key");
  EXPECT_TRUE(value.has_value());
  EXPECT_EQ(123.456, value.value());
}

TEST(ExperimentsUtilsTest, ParseSettingsToDictionaryBoolean) {
  HeapVector<std::pair<WTF::String, Member<V8Union>>> settings;
  settings.push_back(
      std::make_pair("key", MakeGarbageCollected<V8Union>(true)));
  auto result = ParseSettingsToDictionary(settings);
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(1u, result->size());
  absl::optional<bool> value = result->FindBool("key");
  EXPECT_TRUE(value.has_value());
  EXPECT_TRUE(value.value());
}

TEST(ExperimentsUtilsTest, ParseSettingsToDictionaryMixed) {
  HeapVector<std::pair<WTF::String, Member<V8Union>>> settings;
  settings.push_back(std::make_pair(
      "string_key", MakeGarbageCollected<V8Union>(String("string_value"))));
  settings.push_back(
      std::make_pair("long_key", MakeGarbageCollected<V8Union>(123)));
  settings.push_back(
      std::make_pair("double_key", MakeGarbageCollected<V8Union>(123.456)));
  settings.push_back(
      std::make_pair("bool_key", MakeGarbageCollected<V8Union>(true)));

  auto result = ParseSettingsToDictionary(settings);
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(4u, result->size());

  const std::string* string_value = result->FindString("string_key");
  EXPECT_TRUE(string_value);
  EXPECT_EQ("string_value", *string_value);

  absl::optional<int> long_value = result->FindInt("long_key");
  EXPECT_TRUE(long_value.has_value());
  EXPECT_EQ(123, long_value.value());

  absl::optional<double> double_value = result->FindDouble("double_key");
  EXPECT_TRUE(double_value.has_value());
  EXPECT_EQ(123.456, double_value.value());

  absl::optional<bool> bool_value = result->FindBool("bool_key");
  EXPECT_TRUE(bool_value.has_value());
  EXPECT_TRUE(bool_value.value());
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
