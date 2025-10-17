// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/common/background_tracing_state_manager.h"

#include "base/json/json_writer.h"
#include "base/json/values_util.h"
#include "base/strings/pattern.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/tracing/common/pref_names.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tracing {

class BackgroundTracingStateManagerTest : public testing::Test {
 public:
  void SetUp() override {
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    pref_service_->registry()->RegisterBooleanPref(
        metrics::prefs::kMetricsReportingEnabled, false);
    pref_service_->SetBoolean(metrics::prefs::kMetricsReportingEnabled, true);
    tracing::RegisterPrefs(pref_service_->registry());
    state_manager_ = tracing::BackgroundTracingStateManager::CreateInstance(
        pref_service_.get());
  }

  std::string GetSessionStateJson() {
    const base::Value::Dict& state =
        pref_service_->GetDict(tracing::kBackgroundTracingSessionState);

    std::string json;
    EXPECT_TRUE(base::JSONWriter::Write(state, &json));
    return json;
  }

  void SetSessionState(base::Value::Dict dict) {
    pref_service_->Set(tracing::kBackgroundTracingSessionState,
                       base::Value(std::move(dict)));
  }

  void ResetStateManager() {
    state_manager_.reset();
    state_manager_ = tracing::BackgroundTracingStateManager::CreateInstance(
        pref_service_.get());
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<tracing::BackgroundTracingStateManager> state_manager_;
};

TEST_F(BackgroundTracingStateManagerTest, InitializeEmptyPrefs) {
  EXPECT_EQ(GetSessionStateJson(), R"({})");
}

TEST_F(BackgroundTracingStateManagerTest, EnablePrivacyFilter) {
  EXPECT_FALSE(tracing::BackgroundTracingStateManager::GetInstance()
                   .privacy_filter_enabled());

  tracing::BackgroundTracingStateManager::GetInstance().UpdatePrivacyFilter(
      true);

  EXPECT_TRUE(
      base::MatchPattern(GetSessionStateJson(), R"({"privacy_filter":true})"))
      << "Actual: " << GetSessionStateJson();
  EXPECT_TRUE(tracing::BackgroundTracingStateManager::GetInstance()
                  .privacy_filter_enabled());
}

TEST_F(BackgroundTracingStateManagerTest, PrivacyFilterEnabled) {
  base::Value::Dict dict;
  dict.Set("privacy_filter", true);
  SetSessionState(std::move(dict));
  ResetStateManager();

  EXPECT_TRUE(
      base::MatchPattern(GetSessionStateJson(), R"({"privacy_filter":true})"))
      << "Actual: " << GetSessionStateJson();
  EXPECT_TRUE(tracing::BackgroundTracingStateManager::GetInstance()
                  .privacy_filter_enabled());
}

TEST_F(BackgroundTracingStateManagerTest, LoadEnabledScenarios) {
  base::Value::Dict dict;
  dict.Set("enabled_scenarios", base::Value::List().Append("1").Append("3"));
  SetSessionState(std::move(dict));
  ResetStateManager();

  EXPECT_TRUE(base::MatchPattern(GetSessionStateJson(),
                                 R"({"enabled_scenarios":["1","3"]})"))
      << "Actual: " << GetSessionStateJson();
  EXPECT_EQ(std::vector<std::string>({"1", "3"}),
            tracing::BackgroundTracingStateManager::GetInstance()
                .enabled_scenarios());
}

TEST_F(BackgroundTracingStateManagerTest, UpdateEnabledScenarios) {
  tracing::BackgroundTracingStateManager::GetInstance().UpdateEnabledScenarios(
      {"1", "3"});

  EXPECT_TRUE(base::MatchPattern(
      GetSessionStateJson(),
      R"({"enabled_scenarios":["1","3"],"privacy_filter":false})"))
      << "Actual: " << GetSessionStateJson();
  EXPECT_EQ(std::vector<std::string>({"1", "3"}),
            tracing::BackgroundTracingStateManager::GetInstance()
                .enabled_scenarios());
}

}  // namespace tracing
