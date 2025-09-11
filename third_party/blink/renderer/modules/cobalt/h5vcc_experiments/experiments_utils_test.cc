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

  Vector<uint32_t> exp_ids_vector;
  exp_ids_vector.push_back(1001);
  exp_ids_vector.push_back(1002);
  config->setExperimentIds(exp_ids_vector);

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

}  // namespace blink
