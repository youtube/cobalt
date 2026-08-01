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

#include "starboard/shared/starboard/feature_list.h"

#include <iterator>

#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {
using features::FeatureList;
using features::SbFeatureParamExt;

const SbFeature kStarboardFeatureTestEnabled = {"StarboardFeatureTestEnabled",
                                                true};
const SbFeature kStarboardFeatureTestDisabled = {"StarboardFeatureTestDisabled",
                                                 false};

const SbFeature kFeatures[] = {kStarboardFeatureTestEnabled,
                               kStarboardFeatureTestDisabled};

template <typename T>
constexpr SbFeatureParam CreateParam(const char* param_name, T param_value) {
  if constexpr (std::is_same_v<T, bool>) {
    return {"StarboardFeatureTestEnabled",
            param_name,
            SbFeatureParamTypeBool,
            {.bool_value = param_value}};
  } else if constexpr (std::is_same_v<T, int>) {
    return {"StarboardFeatureTestEnabled",
            param_name,
            SbFeatureParamTypeInt,
            {.int_value = param_value}};
  } else if constexpr (std::is_same_v<T, double>) {
    return {"StarboardFeatureTestEnabled",
            param_name,
            SbFeatureParamTypeDouble,
            {.double_value = param_value}};
  } else if constexpr (std::is_convertible_v<T, const char*>) {
    return {"StarboardFeatureTestEnabled",
            param_name,
            SbFeatureParamTypeString,
            {.string_value = param_value}};
  }
}
// A specific overload for the time type to avoid ambiguity with int/int64_t
constexpr SbFeatureParam CreateTimeParam(const char* param_name,
                                         int64_t param_value) {
  return {"StarboardFeatureTestEnabled",
          param_name,
          SbFeatureParamTypeTime,
          {.time_value = param_value}};
}

const SbFeatureParam kParams[] = {
    // Bools
    CreateParam("ParamBoolTrue", true),
    CreateParam("ParamBoolFalse", false),

    // Ints
    CreateParam("ParamIntPositive", 10),
    CreateParam("ParamIntLargePositive", 1234567890),
    CreateParam("ParamIntNegative", -10),
    CreateParam("ParamIntLargeNegative", -1234567890),
    CreateParam("ParamIntZero", 0),

    // Doubles
    CreateParam("ParamDoublePositive", 3.14),
    CreateParam("ParamDoubleLargePositive", 1234567.9876),
    CreateParam("ParamDoubleNegative", -3.14),
    CreateParam("ParamDoubleLargeNegative", -1234567.9876),
    CreateParam("ParamDoubleZero", 0.0),

    // Strings
    CreateParam("ParamStringSunnyDay", "Sunny day"),
    CreateParam("ParamStringEmpty", ""),
    CreateParam("ParamStringWithSymbols", "width=1920; height=1080;"),

    // Time
    CreateTimeParam("ParamTime", 123456789),
};

int g_callback_before_init_count = 0;
void CallbackBeforeInit() {
  ++g_callback_before_init_count;
}

struct RegisterBeforeInit {
  RegisterBeforeInit() {
    FeatureList::RegisterFeaturesInitializedCallback(&CallbackBeforeInit);
  }
};
const RegisterBeforeInit g_register_before_init;

int g_callback_after_init_count = 0;
void CallbackAfterInit() {
  ++g_callback_after_init_count;
}

}  // namespace

class StarboardFeatureListTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    FeatureList::InitializeFeatureList(kFeatures, std::size(kFeatures), kParams,
                                       std::size(kParams));
  }
};

TEST_F(StarboardFeatureListTest, CallbackRegisteredBeforeInitializationIsRun) {
  EXPECT_EQ(g_callback_before_init_count, 1);
}

TEST_F(StarboardFeatureListTest, CallbackRegisteredAfterInitializationRunsNow) {
  const int count_before = g_callback_after_init_count;
  FeatureList::RegisterFeaturesInitializedCallback(&CallbackAfterInit);
  // The features are already initialized, so the callback must have run
  // synchronously on this thread.
  EXPECT_EQ(g_callback_after_init_count, count_before + 1);
}

TEST_F(StarboardFeatureListTest, CallbackCanReadFeatures) {
  // Callbacks run with mutex_ released, so reading a feature from one must
  // not deadlock.
  FeatureList::RegisterFeaturesInitializedCallback([] {
    EXPECT_TRUE(FeatureList::IsEnabled(kStarboardFeatureTestEnabled));
  });
}

TEST_F(StarboardFeatureListTest, CanAccessFeatures) {
  EXPECT_TRUE(FeatureList::IsEnabled(kStarboardFeatureTestEnabled));
  EXPECT_FALSE(FeatureList::IsEnabled(kStarboardFeatureTestDisabled));
}

TEST_F(StarboardFeatureListTest, CanAccessParams) {
  for (const auto& param : kParams) {
    switch (param.type) {
      case SbFeatureParamTypeBool: {
        auto key = SbFeatureParamExt<bool>(kStarboardFeatureTestEnabled,
                                           param.param_name);
        EXPECT_EQ(key.Get(), param.value.bool_value);
        break;
      }
      case SbFeatureParamTypeInt: {
        auto key = SbFeatureParamExt<int>(kStarboardFeatureTestEnabled,
                                          param.param_name);
        EXPECT_EQ(key.Get(), param.value.int_value);
        break;
      }
      case SbFeatureParamTypeDouble: {
        auto key = SbFeatureParamExt<double>(kStarboardFeatureTestEnabled,
                                             param.param_name);
        EXPECT_DOUBLE_EQ(key.Get(), param.value.double_value);
        break;
      }
      case SbFeatureParamTypeString: {
        auto key = SbFeatureParamExt<std::string>(kStarboardFeatureTestEnabled,
                                                  param.param_name);
        EXPECT_EQ(key.Get(), param.value.string_value);
        break;
      }
      case SbFeatureParamTypeTime: {
        auto key = SbFeatureParamExt<int64_t>(kStarboardFeatureTestEnabled,
                                              param.param_name);
        EXPECT_EQ(key.Get(), param.value.time_value);
        break;
      }
      default:
        FAIL() << "Unhandled parameter type for param: " << param.param_name;
    }
  }
}
}  // namespace starboard
