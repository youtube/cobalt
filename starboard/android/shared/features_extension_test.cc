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

#include "starboard/android/shared/features_extension.h"

#include <any>
#include <iterator>
#include <variant>

#include "starboard/shared/starboard/feature_list.h"
#include "starboard/shared/starboard/features.h"
#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard::android::shared {
namespace {
using starboard::features::FeatureList;
using starboard::features::SbFeatureParamExt;

using ParamValue = std::variant<bool, int, double, std::string, int64_t>;

const SbFeature FeatureTestBoolTrue = {"FeatureTestBoolTrue", true};
const SbFeature FeatureTestBoolFalse = {"FeatureTestBoolFalse", false};

const SbFeatureParam ParamTestBool = {"FeatureTestBoolTrue",
                                      "ParamBool",
                                      SbFeatureParamTypeBool,
                                      {.bool_value = true}};
const SbFeatureParam ParamTestInt = {"FeatureTestBoolTrue",
                                     "ParamInt",
                                     SbFeatureParamTypeInt,
                                     {.int_value = 10}};
const SbFeatureParam ParamTestDouble = {"FeatureTestBoolTrue",
                                        "ParamDouble",
                                        SbFeatureParamTypeDouble,
                                        {.double_value = 3.14}};
const SbFeatureParam ParamTestString = {"FeatureTestBoolTrue",
                                        "ParamString",
                                        SbFeatureParamTypeString,
                                        {.string_value = "Sunny day"}};
const SbFeatureParam ParamTestTime = {"FeatureTestBoolTrue",
                                      "ParamTime",
                                      SbFeatureParamTypeTime,
                                      {.time_value = 123456789}};

const SbFeature features[] = {FeatureTestBoolTrue, FeatureTestBoolFalse};
const SbFeatureParam params[] = {ParamTestBool, ParamTestInt, ParamTestDouble,
                                 ParamTestString, ParamTestTime};

struct ParamTestCase {
  std::string test_name;
  SbFeatureParam param_definition;
  ParamValue expected_value;
};

}  // namespace

class FeatureExtensionTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    const StarboardExtensionFeaturesApi* extension_api =
        static_cast<const StarboardExtensionFeaturesApi*>(
            SbSystemGetExtension(kStarboardExtensionFeaturesName));

    extension_api->InitializeStarboardFeatures(features, std::size(features),
                                               params, std::size(params));
  }
};

TEST_F(FeatureExtensionTest, CanAccessFeatures) {
  EXPECT_TRUE(FeatureList::IsEnabled(FeatureTestBoolTrue));
  EXPECT_FALSE(FeatureList::IsEnabled(FeatureTestBoolFalse));
}

class ParamExtensionTest : public ::testing::Test,
                           public ::testing::WithParamInterface<ParamTestCase> {
};

TEST_P(ParamExtensionTest, CanAccessParams) {
  const auto& test_case = GetParam();
  const auto& param_def = test_case.param_definition;

  // std::visit will select the right code to run based on the
  // type currently held in the 'expected_value' variant.
  std::visit(
      [&](const auto& expected_value) {
        // Get the type T from the expected_value (e.g., bool, int, ...)
        using T = std::decay_t<decltype(expected_value)>;

        // The logic is now generic and uses the deduced type T
        auto key =
            SbFeatureParamExt<T>(FeatureTestBoolTrue, param_def.param_name);
        auto actual_value = FeatureList::GetParam(key);

        EXPECT_EQ(actual_value, expected_value);
      },
      test_case.expected_value);
}

INSTANTIATE_TEST_SUITE_P(
    ParamTests,
    ParamExtensionTest,
    ::testing::Values(
        ParamTestCase{"Bool", ParamTestBool, ParamTestBool.value.bool_value},
        ParamTestCase{"Int", ParamTestInt, ParamTestInt.value.int_value},
        ParamTestCase{"Double", ParamTestDouble,
                      ParamTestDouble.value.double_value},
        ParamTestCase{"String", ParamTestString,
                      ParamTestString.value.string_value},
        ParamTestCase{"Time", ParamTestTime, ParamTestTime.value.time_value}),
    // This lambda tells gtest how to name each test case
    [](const auto& info) { return info.param.test_name; });

}  // namespace starboard::android::shared
