// Copyright 2025 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/browser/metrics/cobalt_enabled_state_provider.h"

#include "base/command_line.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_switches.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {

using metrics::prefs::kMetricsReportingEnabled;

class CobaltEnabledStateProviderTest : public testing::Test {
 protected:
  CobaltEnabledStateProviderTest() = default;

  void SetUp() override {
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    pref_service_->registry()->RegisterBooleanPref(kMetricsReportingEnabled,
                                                   false);
    state_provider_ =
        std::make_unique<CobaltEnabledStateProvider>(pref_service_.get());
    command_line_ = base::CommandLine::ForCurrentProcess();
  }

  TestingPrefServiceSimple* pref_service() { return pref_service_.get(); }
  CobaltEnabledStateProvider* state_provider() { return state_provider_.get(); }
  base::CommandLine* command_line() { return command_line_; }

 private:
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<CobaltEnabledStateProvider> state_provider_;
  base::CommandLine* command_line_;
};

TEST_F(CobaltEnabledStateProviderTest, DefaultConsentIsFalse) {
  EXPECT_FALSE(state_provider()->IsConsentGiven());
}

TEST_F(CobaltEnabledStateProviderTest, DefaultReportingIsEnabledIsFalse) {
  EXPECT_FALSE(state_provider()->IsReportingEnabled());
}

TEST_F(CobaltEnabledStateProviderTest, SetConsentGivenTrue) {
  state_provider()->SetConsentGiven(true);
  EXPECT_TRUE(pref_service()->GetBoolean(kMetricsReportingEnabled));
  EXPECT_TRUE(state_provider()->IsConsentGiven());
  EXPECT_TRUE(state_provider()->IsReportingEnabled());
}

TEST_F(CobaltEnabledStateProviderTest, SetConsentGivenFalse) {
  pref_service()->SetBoolean(kMetricsReportingEnabled, true);
  state_provider()->SetConsentGiven(false);
  EXPECT_FALSE(pref_service()->GetBoolean(kMetricsReportingEnabled));
  EXPECT_FALSE(state_provider()->IsConsentGiven());
  EXPECT_FALSE(state_provider()->IsReportingEnabled());
}

TEST_F(CobaltEnabledStateProviderTest, SetReportingEnabledTrue) {
  state_provider()->SetReportingEnabled(true);
  EXPECT_TRUE(pref_service()->GetBoolean(kMetricsReportingEnabled));
  EXPECT_TRUE(state_provider()->IsConsentGiven());
  EXPECT_TRUE(state_provider()->IsReportingEnabled());
}

TEST_F(CobaltEnabledStateProviderTest, SetReportingEnabledFalse) {
  pref_service()->SetBoolean(kMetricsReportingEnabled, true);
  state_provider()->SetReportingEnabled(false);
  EXPECT_FALSE(pref_service()->GetBoolean(kMetricsReportingEnabled));
  EXPECT_FALSE(state_provider()->IsConsentGiven());
  EXPECT_FALSE(state_provider()->IsReportingEnabled());
}

TEST_F(CobaltEnabledStateProviderTest, CommandLineOverrideEnabled) {
  command_line()->AppendSwitch(metrics::switches::kForceEnableMetricsReporting);
  EXPECT_TRUE(state_provider()->IsConsentGiven());
  EXPECT_TRUE(state_provider()->IsReportingEnabled());
}

TEST_F(CobaltEnabledStateProviderTest, CommandLineOverrideTakesPrecedence) {
  pref_service()->SetBoolean(kMetricsReportingEnabled, false);
  command_line()->AppendSwitch(metrics::switches::kForceEnableMetricsReporting);
  EXPECT_TRUE(state_provider()->IsConsentGiven());
  EXPECT_TRUE(state_provider()->IsReportingEnabled());

  pref_service()->SetBoolean(kMetricsReportingEnabled, true);
  EXPECT_TRUE(state_provider()->IsConsentGiven());
  EXPECT_TRUE(state_provider()->IsReportingEnabled());
}

}  // namespace cobalt
