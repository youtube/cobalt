// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/tracing/aw_tracing_delegate.h"

#include "android_webview/browser/aw_browser_process.h"
#include "android_webview/browser/aw_feature_list_creator.h"
#include "base/values.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/tracing/common/background_tracing_state_manager.h"
#include "components/tracing/common/pref_names.h"
#include "content/public/browser/background_tracing_config.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android_webview {

class AwTracingDelegateTest : public testing::Test {
 public:
  void SetUp() override {
    AwFeatureListCreator* aw_feature_list_creator = new AwFeatureListCreator();
    aw_feature_list_creator->CreateLocalState();
    browser_process_ =
        new android_webview::AwBrowserProcess(aw_feature_list_creator);

    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    pref_service_->registry()->RegisterBooleanPref(
        metrics::prefs::kMetricsReportingEnabled, false);
    pref_service_->SetBoolean(metrics::prefs::kMetricsReportingEnabled, true);
    tracing::RegisterPrefs(pref_service_->registry());
    tracing::BackgroundTracingStateManager::GetInstance()
        .SetPrefServiceForTesting(pref_service_.get());
  }

  void TearDown() override {
    delete browser_process_;
    tracing::BackgroundTracingStateManager::GetInstance().Reset();
  }

  android_webview::AwTracingDelegate delegate_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  raw_ptr<android_webview::AwBrowserProcess> browser_process_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
};

std::unique_ptr<content::BackgroundTracingConfig> CreateValidConfig() {
  base::Value::Dict dict;
  dict.Set("scenario_name", "TestScenario");
  dict.Set("mode", "PREEMPTIVE_TRACING_MODE");
  dict.Set("custom_categories", "toplevel");
  base::Value::List rules_list;

  {
    base::Value::Dict rules_dict;
    rules_dict.Set("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED");
    rules_dict.Set("trigger_name", "test");
    rules_list.Append(std::move(rules_dict));
  }

  dict.Set("configs", std::move(rules_list));
  return content::BackgroundTracingConfig::FromDict(std::move(dict));
}

TEST_F(AwTracingDelegateTest, IsAllowedToBegin) {
  auto config = CreateValidConfig();

  EXPECT_TRUE(delegate_.IsAllowedToBeginBackgroundScenario(
      config->scenario_name(), /*requires_anonymized_data=*/false,
      /*is_crash_scenario=*/false));
  EXPECT_TRUE(delegate_.IsAllowedToEndBackgroundScenario(
      config->scenario_name(), /*requires_anonymized_data=*/false,
      /*is_crash_scenario=*/false));
}

TEST_F(AwTracingDelegateTest, IsAllowedToBeginSessionEndedUnexpectedly) {
  tracing::BackgroundTracingStateManager::GetInstance().SaveState(
      {}, tracing::BackgroundTracingState::STARTED);

  base::Value dict(base::Value::Type::DICT);
  tracing::BackgroundTracingStateManager::GetInstance().Initialize(nullptr);

  auto config = CreateValidConfig();

  EXPECT_FALSE(delegate_.IsAllowedToBeginBackgroundScenario(
      config->scenario_name(), /*requires_anonymized_data=*/false,
      /*is_crash_scenario=*/false));
}

TEST_F(AwTracingDelegateTest, IsAllowedToBeginRecentlyUploaded) {
  tracing::BackgroundTracingStateManager::GetInstance().Initialize(nullptr);
  tracing::BackgroundTracingStateManager::GetInstance().OnScenarioUploaded(
      "TestScenario");

  auto config = CreateValidConfig();
  EXPECT_FALSE(delegate_.IsAllowedToBeginBackgroundScenario(
      config->scenario_name(), /*requires_anonymized_data=*/false,
      /*is_crash_scenario=*/false));
}

TEST_F(AwTracingDelegateTest, IsAllowedToEndRecentlyUploaded) {
  tracing::BackgroundTracingStateManager::GetInstance().Initialize(nullptr);
  tracing::BackgroundTracingStateManager::GetInstance().OnScenarioUploaded(
      "TestScenario");

  auto config = CreateValidConfig();
  EXPECT_FALSE(delegate_.IsAllowedToEndBackgroundScenario(
      config->scenario_name(), /*requires_anonymized_data=*/false,
      /*is_crash_scenario=*/false));
}

}  // namespace android_webview