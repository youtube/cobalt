// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/field_trial_config/field_trial_util.h"

#include <map>
#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/stl_util.h"
#include "base/test/scoped_feature_list.h"
#include "components/variations/field_trial_config/fieldtrial_testing_config.h"
#include "components/variations/variations_associated_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {
namespace {

class FieldTrialUtilTest : public ::testing::Test {
 public:
  FieldTrialUtilTest() : field_trial_list_(nullptr) {}

  ~FieldTrialUtilTest() override {
    // Ensure that the maps are cleared between tests, since they are stored as
    // process singletons.
    testing::ClearAllVariationIDs();
    testing::ClearAllVariationParams();
  }

 private:
  base::FieldTrialList field_trial_list_;

  DISALLOW_COPY_AND_ASSIGN(FieldTrialUtilTest);
};

}  // namespace

TEST_F(FieldTrialUtilTest, AssociateParamsFromString) {
  const std::string kTrialName = "AssociateVariationParams";
  const std::string kVariationsString =
      "AssociateVariationParams.A:a/10/b/test,AssociateVariationParams.B:a/%2F";
  ASSERT_TRUE(AssociateParamsFromString(kVariationsString));

  base::FieldTrialList::CreateFieldTrial(kTrialName, "B");
  EXPECT_EQ("/", GetVariationParamValue(kTrialName, "a"));
  EXPECT_EQ(std::string(), GetVariationParamValue(kTrialName, "b"));
  EXPECT_EQ(std::string(), GetVariationParamValue(kTrialName, "x"));

  std::map<std::string, std::string> params;
  EXPECT_TRUE(GetVariationParams(kTrialName, &params));
  EXPECT_EQ(1U, params.size());
  EXPECT_EQ("/", params["a"]);
}

TEST_F(FieldTrialUtilTest, AssociateParamsFromStringWithSameTrial) {
  const std::string kTrialName = "AssociateVariationParams";
  const std::string kVariationsString =
      "AssociateVariationParams.A:a/10/b/test,AssociateVariationParams.A:a/x";
  ASSERT_FALSE(AssociateParamsFromString(kVariationsString));
}

TEST_F(FieldTrialUtilTest, AssociateParamsFromFieldTrialConfig) {
  const Study::Platform platform = Study::PLATFORM_LINUX;
  const FieldTrialTestingExperimentParams array_kFieldTrialConfig_params_0[] =
      {{"x", "1"}, {"y", "2"}};
  const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments_0[] = {
      {"TestGroup1", &platform, 1, array_kFieldTrialConfig_params_0, 2,
       nullptr, 0, nullptr, 0, nullptr},
  };
  const FieldTrialTestingExperimentParams array_kFieldTrialConfig_params_1[] =
      {{"x", "3"}, {"y", "4"}};
  const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments_1[] = {
      {"TestGroup2", &platform, 1, array_kFieldTrialConfig_params_0, 2,
       nullptr, 0, nullptr, 0, nullptr},
      {"TestGroup2-2", &platform, 1, array_kFieldTrialConfig_params_1, 2,
       nullptr, 0, nullptr, 0, nullptr},
  };
  const FieldTrialTestingStudy array_kFieldTrialConfig_studies[] = {
      {"TestTrial1", array_kFieldTrialConfig_experiments_0, 1},
      {"TestTrial2", array_kFieldTrialConfig_experiments_1, 2},
  };
  const FieldTrialTestingConfig kConfig = {
      array_kFieldTrialConfig_studies, 2
  };

  base::FeatureList feature_list;
  AssociateParamsFromFieldTrialConfig(kConfig, &feature_list, platform);

  EXPECT_EQ("1", GetVariationParamValue("TestTrial1", "x"));
  EXPECT_EQ("2", GetVariationParamValue("TestTrial1", "y"));

  std::map<std::string, std::string> params;
  EXPECT_TRUE(GetVariationParams("TestTrial1", &params));
  EXPECT_EQ(2U, params.size());
  EXPECT_EQ("1", params["x"]);
  EXPECT_EQ("2", params["y"]);

  EXPECT_EQ("TestGroup1", base::FieldTrialList::FindFullName("TestTrial1"));
  EXPECT_EQ("TestGroup2", base::FieldTrialList::FindFullName("TestTrial2"));
}

TEST_F(FieldTrialUtilTest,
       AssociateParamsFromFieldTrialConfigWithEachPlatform) {
  const Study::Platform all_platforms[] = {
      Study::PLATFORM_ANDROID,
      Study::PLATFORM_ANDROID_WEBVIEW,
      Study::PLATFORM_CHROMEOS,
      Study::PLATFORM_FUCHSIA,
      Study::PLATFORM_IOS,
      Study::PLATFORM_LINUX,
      Study::PLATFORM_MAC,
      Study::PLATFORM_WINDOWS,
  };

  // Break if platforms are added without updating |all_platforms|.
  static_assert(base::size(all_platforms) == Study::Platform_ARRAYSIZE,
                "|all_platforms| must include all platforms.");

  const FieldTrialTestingExperimentParams array_kFieldTrialConfig_params[] =
      {{"x", "1"}, {"y", "2"}};

  for (size_t i = 0; i < base::size(all_platforms); ++i) {
    const Study::Platform platform = all_platforms[i];
    const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments[] = {
        {"TestGroup", &platform, 1,
         array_kFieldTrialConfig_params, 2, nullptr, 0, nullptr, 0, nullptr},
    };
    const FieldTrialTestingStudy array_kFieldTrialConfig_studies[] = {
        {"TestTrial", array_kFieldTrialConfig_experiments, 1}
    };
    const FieldTrialTestingConfig kConfig = {
        array_kFieldTrialConfig_studies, 1
    };

    base::FeatureList feature_list;
    AssociateParamsFromFieldTrialConfig(kConfig, &feature_list, platform);

    EXPECT_EQ("1", GetVariationParamValue("TestTrial", "x"));
    EXPECT_EQ("2", GetVariationParamValue("TestTrial", "y"));

    std::map<std::string, std::string> params;
    EXPECT_TRUE(GetVariationParams("TestTrial", &params));
    EXPECT_EQ(2U, params.size());
    EXPECT_EQ("1", params["x"]);
    EXPECT_EQ("2", params["y"]);

    EXPECT_EQ("TestGroup", base::FieldTrialList::FindFullName("TestTrial"));
  }
}

TEST_F(FieldTrialUtilTest,
       AssociateParamsFromFieldTrialConfigWithDifferentPlatform) {
  const Study::Platform platform = Study::PLATFORM_ANDROID;
  const FieldTrialTestingExperimentParams array_kFieldTrialConfig_params[] =
      {{"x", "1"}, {"y", "2"}};
  const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments[] = {
      {"TestGroup", &platform, 1, array_kFieldTrialConfig_params, 2, nullptr, 0,
       nullptr, 0, nullptr},
  };
  const FieldTrialTestingStudy array_kFieldTrialConfig_studies[] =
      {{"TestTrial", array_kFieldTrialConfig_experiments, 1}};
  const FieldTrialTestingConfig kConfig =
      {array_kFieldTrialConfig_studies, 1};

  // The platforms don't match, so trial shouldn't be added.
  base::FeatureList feature_list;
  AssociateParamsFromFieldTrialConfig(kConfig, &feature_list,
                                      Study::PLATFORM_ANDROID_WEBVIEW);

  EXPECT_EQ("", GetVariationParamValue("TestTrial", "x"));
  EXPECT_EQ("", GetVariationParamValue("TestTrial", "y"));

  std::map<std::string, std::string> params;
  EXPECT_FALSE(GetVariationParams("TestTrial", &params));

  EXPECT_EQ("", base::FieldTrialList::FindFullName("TestTrial"));
}

TEST_F(FieldTrialUtilTest,
       AssociateParamsFromFieldTrialConfigWithMultiplePlatforms) {
  const Study::Platform platforms[] =
      {Study::PLATFORM_ANDROID, Study::PLATFORM_ANDROID_WEBVIEW};
  const FieldTrialTestingExperimentParams array_kFieldTrialConfig_params[] =
      {{"x", "1"}, {"y", "2"}};
  const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments[] = {
      {"TestGroup", platforms, 2, array_kFieldTrialConfig_params, 2, nullptr, 0,
       nullptr, 0, nullptr},
  };
  const FieldTrialTestingStudy array_kFieldTrialConfig_studies[] =
      {{"TestTrial", array_kFieldTrialConfig_experiments, 1}};
  const FieldTrialTestingConfig kConfig =
      {array_kFieldTrialConfig_studies, 1};

  // One of the platforms matches, so trial should be added.
  base::FeatureList feature_list;
  AssociateParamsFromFieldTrialConfig(kConfig, &feature_list,
                                      Study::PLATFORM_ANDROID_WEBVIEW);

  EXPECT_EQ("1", GetVariationParamValue("TestTrial", "x"));
  EXPECT_EQ("2", GetVariationParamValue("TestTrial", "y"));

  std::map<std::string, std::string> params;
  EXPECT_TRUE(GetVariationParams("TestTrial", &params));
  EXPECT_EQ(2U, params.size());
  EXPECT_EQ("1", params["x"]);
  EXPECT_EQ("2", params["y"]);

  EXPECT_EQ("TestGroup", base::FieldTrialList::FindFullName("TestTrial"));
}

TEST_F(FieldTrialUtilTest, AssociateFeaturesFromFieldTrialConfig) {
  const base::Feature kFeatureA{"A", base::FEATURE_DISABLED_BY_DEFAULT};
  const base::Feature kFeatureB{"B", base::FEATURE_ENABLED_BY_DEFAULT};
  const base::Feature kFeatureC{"C", base::FEATURE_DISABLED_BY_DEFAULT};
  const base::Feature kFeatureD{"D", base::FEATURE_ENABLED_BY_DEFAULT};

  const char* enable_features[] = {"A", "B"};
  const char* disable_features[] = {"C", "D"};

  const Study::Platform platform = Study::PLATFORM_LINUX;
  const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments_0[] = {
      {"TestGroup1", &platform, 1, nullptr, 0, enable_features, 2, nullptr, 0,
       nullptr},
  };
  const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments_1[] = {
      {"TestGroup2", &platform, 1, nullptr, 0, nullptr, 0, disable_features, 2,
       nullptr},
      {"TestGroup2-2", &platform, 1, nullptr, 0, nullptr, 0, nullptr, 0,
       nullptr},
  };

  const FieldTrialTestingStudy array_kFieldTrialConfig_studies[] = {
      {"TestTrial1", array_kFieldTrialConfig_experiments_0, 1},
      {"TestTrial2", array_kFieldTrialConfig_experiments_1, 2},
  };

  const FieldTrialTestingConfig kConfig = {
      array_kFieldTrialConfig_studies, 2
  };

  std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
  AssociateParamsFromFieldTrialConfig(kConfig, feature_list.get(), platform);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  // Check the resulting feature and field trial states. Trials should not be
  // active until their associated features are queried.
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive("TestTrial1"));
  EXPECT_TRUE(base::FeatureList::IsEnabled(kFeatureA));
  EXPECT_TRUE(base::FeatureList::IsEnabled(kFeatureB));
  EXPECT_TRUE(base::FieldTrialList::IsTrialActive("TestTrial1"));

  EXPECT_FALSE(base::FieldTrialList::IsTrialActive("TestTrial2"));
  EXPECT_FALSE(base::FeatureList::IsEnabled(kFeatureC));
  EXPECT_FALSE(base::FeatureList::IsEnabled(kFeatureD));
  EXPECT_TRUE(base::FieldTrialList::IsTrialActive("TestTrial2"));
}

TEST_F(FieldTrialUtilTest, AssociateForcingFlagsFromFieldTrialConfig) {
  const Study::Platform platform = Study::PLATFORM_LINUX;
  const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments_0[] = {
      {"TestGroup1", &platform, 1, nullptr, 0, nullptr, 0, nullptr, 0, nullptr}
  };
  const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments_1[] = {
      {"TestGroup2", &platform, 1, nullptr, 0, nullptr, 0, nullptr, 0,
       nullptr},
      {"ForcedGroup2", &platform, 1, nullptr, 0, nullptr, 0, nullptr, 0,
       "flag-2"},
  };
  const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments_2[] = {
      {"TestGroup3", &platform, 1, nullptr, 0, nullptr, 0, nullptr, 0,
       nullptr},
      {"ForcedGroup3", &platform, 1, nullptr, 0, nullptr, 0, nullptr, 0,
       "flag-3"},
      {"ForcedGroup3-2", &platform, 1, nullptr, 0, nullptr, 0, nullptr, 0,
       "flag-3-2"},
  };
  const FieldTrialTestingStudy array_kFieldTrialConfig_studies[] = {
      {"TestTrial1", array_kFieldTrialConfig_experiments_0, 1},
      {"TestTrial2", array_kFieldTrialConfig_experiments_1, 2},
      {"TestTrial3", array_kFieldTrialConfig_experiments_2, 3},
  };
  const FieldTrialTestingConfig kConfig = {
      array_kFieldTrialConfig_studies, 3
  };

  base::CommandLine::ForCurrentProcess()->AppendSwitch("flag-2");
  base::CommandLine::ForCurrentProcess()->AppendSwitch("flag-3");

  base::FeatureList feature_list;
  AssociateParamsFromFieldTrialConfig(kConfig, &feature_list, platform);

  EXPECT_EQ("TestGroup1", base::FieldTrialList::FindFullName("TestTrial1"));
  EXPECT_EQ("ForcedGroup2", base::FieldTrialList::FindFullName("TestTrial2"));
  EXPECT_EQ("ForcedGroup3", base::FieldTrialList::FindFullName("TestTrial3"));
}

TEST_F(FieldTrialUtilTest, TestEscapeValue) {
  std::string str = "trail.:/,*";
  std::string escaped_str = EscapeValue(str);
  EXPECT_EQ(escaped_str.find('.'), std::string::npos);
  EXPECT_EQ(escaped_str.find(':'), std::string::npos);
  EXPECT_EQ(escaped_str.find('/'), std::string::npos);
  EXPECT_EQ(escaped_str.find(','), std::string::npos);
  EXPECT_EQ(escaped_str.find('*'), std::string::npos);

  EXPECT_EQ(str, UnescapeValue(escaped_str));
}
}  // namespace variations
