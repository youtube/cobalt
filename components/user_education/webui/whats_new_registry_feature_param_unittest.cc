// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "components/user_education/common/user_education_features.h"
#include "components/user_education/webui/mock_whats_new_storage_service.h"
#include "components/user_education/webui/whats_new_registry.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/expect_call_in_scope.h"

using testing::Return;
using whats_new::WhatsNewEdition;
using whats_new::WhatsNewModule;
using whats_new::WhatsNewRegistry;
using whats_new::WhatsNewStorageService;

namespace user_education {

namespace {

using BrowserCommand = browser_command::mojom::Command;

// Modules
// Enabled through feature list.
BASE_FEATURE(kTestModuleEnabled1,
             "TestModuleEnabled1",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTestModuleEnabled2,
             "TestModuleEnabled2",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTestEditionEnabled,
             "TestEditionEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Enabled by default
BASE_FEATURE(kTestModuleEnabled3,
             "TestModuleEnabled3",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace

class WhatsNewRegistryFeatureParamTest : public testing::Test {
 public:
  WhatsNewRegistryFeatureParamTest() = default;
  ~WhatsNewRegistryFeatureParamTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();
    feature_list_.Reset();
  }

  void CreateRegistry() {
    auto mock_storage_service = std::make_unique<MockWhatsNewStorageService>();
    EXPECT_CALL(*mock_storage_service, SetModuleEnabled(testing::_)).Times(3);
    EXPECT_CALL(*mock_storage_service, ReadModuleData())
        .WillRepeatedly(testing::ReturnRef(stored_enabled_modules_));
    whats_new_registry_ =
        std::make_unique<WhatsNewRegistry>(std::move(mock_storage_service));

    // Modules
    whats_new_registry_->RegisterModule(
        WhatsNewModule(kTestModuleEnabled1, ""));
    stored_enabled_modules_.Append(kTestModuleEnabled1.name);
    whats_new_registry_->RegisterModule(
        WhatsNewModule(kTestModuleEnabled2, ""));
    stored_enabled_modules_.Append(kTestModuleEnabled2.name);
    whats_new_registry_->RegisterModule(
        WhatsNewModule(kTestModuleEnabled3, ""));
    stored_enabled_modules_.Append(kTestModuleEnabled3.name);

    // Editions
    whats_new_registry_->RegisterEdition(
        WhatsNewEdition(kTestEditionEnabled, ""));
  }

  void TearDown() override {
    stored_enabled_modules_.clear();
    whats_new_registry_.reset();
    testing::Test::TearDown();
  }

 protected:
  std::unique_ptr<WhatsNewRegistry> whats_new_registry_;
  base::Value::List stored_enabled_modules_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(WhatsNewRegistryFeatureParamTest, FeatureWithoutCustomization) {
  feature_list_.InitWithFeatures({kTestModuleEnabled1, kTestModuleEnabled2},
                                 {});
  CreateRegistry();

  std::string customization1 = base::GetFieldTrialParamValueByFeature(
      kTestModuleEnabled1, whats_new::kCustomizationParam);
  EXPECT_EQ("", customization1);

  std::string customization2 = base::GetFieldTrialParamValueByFeature(
      kTestModuleEnabled2, whats_new::kCustomizationParam);
  EXPECT_EQ("", customization2);

  auto active_customizations = whats_new_registry_->GetCustomizations();
  EXPECT_EQ(static_cast<size_t>(0), active_customizations.size());
}

TEST_F(WhatsNewRegistryFeatureParamTest, FeatureWithCustomization) {
  feature_list_.InitWithFeaturesAndParameters(
      {{kTestModuleEnabled1, {{whats_new::kCustomizationParam, "hello"}}},
       {kTestModuleEnabled2, {{}}}},
      {});
  CreateRegistry();

  std::string customization1 = base::GetFieldTrialParamValueByFeature(
      kTestModuleEnabled1, whats_new::kCustomizationParam);
  EXPECT_EQ("hello", customization1);

  std::string customization2 = base::GetFieldTrialParamValueByFeature(
      kTestModuleEnabled2, whats_new::kCustomizationParam);
  EXPECT_EQ("", customization2);

  auto active_customizations = whats_new_registry_->GetCustomizations();
  EXPECT_EQ(static_cast<size_t>(1), active_customizations.size());
  EXPECT_EQ("hello", active_customizations.at(0));
}

TEST_F(WhatsNewRegistryFeatureParamTest, MultipleFeaturesWithCustomizations) {
  feature_list_.InitWithFeaturesAndParameters(
      {{kTestModuleEnabled1, {{whats_new::kCustomizationParam, "hello"}}},
       {kTestModuleEnabled2, {{whats_new::kCustomizationParam, "goodbye"}}}},
      {});
  CreateRegistry();

  std::string customization1 = base::GetFieldTrialParamValueByFeature(
      kTestModuleEnabled1, whats_new::kCustomizationParam);
  EXPECT_EQ("hello", customization1);

  std::string customization2 = base::GetFieldTrialParamValueByFeature(
      kTestModuleEnabled2, whats_new::kCustomizationParam);
  EXPECT_EQ("goodbye", customization2);

  auto active_customizations = whats_new_registry_->GetCustomizations();
  EXPECT_EQ(static_cast<size_t>(2), active_customizations.size());
  EXPECT_EQ("hello", active_customizations.at(0));
  EXPECT_EQ("goodbye", active_customizations.at(1));
}

TEST_F(WhatsNewRegistryFeatureParamTest, FeatureWithoutSurvey) {
  feature_list_.InitWithFeatures({kTestModuleEnabled1, kTestModuleEnabled2},
                                 {});
  CreateRegistry();

  std::string survey_param = base::GetFieldTrialParamValueByFeature(
      kTestEditionEnabled, whats_new::kSurveyParam);
  EXPECT_EQ("", survey_param);

  auto survey_from_registry = whats_new_registry_->GetActiveEditionSurvey();
  EXPECT_FALSE(survey_from_registry.has_value());
}

TEST_F(WhatsNewRegistryFeatureParamTest, FeatureWithSurvey) {
  feature_list_.InitWithFeaturesAndParameters(
      {{kTestModuleEnabled1, {{}}},
       {kTestModuleEnabled2, {{}}},
       {kTestEditionEnabled, {{whats_new::kSurveyParam, "hello"}}}},
      {});
  CreateRegistry();

  std::string survey_param = base::GetFieldTrialParamValueByFeature(
      kTestEditionEnabled, whats_new::kSurveyParam);
  EXPECT_EQ("hello", survey_param);

  auto survey_from_registry = whats_new_registry_->GetActiveEditionSurvey();
  EXPECT_TRUE(survey_from_registry.has_value());
  EXPECT_EQ("hello", survey_from_registry.value());
}

}  // namespace user_education
