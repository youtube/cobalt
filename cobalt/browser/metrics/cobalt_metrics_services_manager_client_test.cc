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

#include "cobalt/browser/metrics/cobalt_metrics_services_manager_client.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "cobalt/browser/metrics/cobalt_enabled_state_provider.h"
#include "cobalt/browser/metrics/cobalt_metrics_service_client.h"
#include "cobalt/shell/browser/shell_paths.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/metrics_switches.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/synthetic_trial_registry.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::IsNull;
using testing::NotNull;

namespace cobalt {

class CobaltMetricsServicesManagerClientTest : public ::testing::Test {
 protected:
  CobaltMetricsServicesManagerClientTest() = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    path_override_ = std::make_unique<base::ScopedPathOverride>(
        base::DIR_CACHE, temp_dir_.GetPath());

    metrics::MetricsService::RegisterPrefs(prefs_.registry());
    // Add any other prefs that might be accessed.
    prefs_.registry()->RegisterBooleanPref(
        metrics::prefs::kMetricsReportingEnabled, false);

    // Make sure user_metrics.cc has a g_task_runner.
    base::SetRecordActionTaskRunner(
        task_environment_.GetMainThreadTaskRunner());

    manager_client_ =
        std::make_unique<CobaltMetricsServicesManagerClient>(&prefs_);
  }

  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple prefs_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<base::ScopedPathOverride> path_override_;
  std::unique_ptr<CobaltMetricsServicesManagerClient> manager_client_;
};

TEST_F(CobaltMetricsServicesManagerClientTest, ConstructorInitializesState) {
  EXPECT_THAT(manager_client_->GetEnabledStateProvider(), NotNull());
  EXPECT_FALSE(manager_client_->IsMetricsReportingEnabled());
  EXPECT_FALSE(manager_client_->IsMetricsConsentGiven());
  EXPECT_THAT(manager_client_->metrics_service_client(), IsNull());
}

TEST_F(CobaltMetricsServicesManagerClientTest,
       IsMetricsReportingEnabledReflectsPref) {
  // Case 1: Pref is false (default from SetUp).
  prefs_.SetBoolean(metrics::prefs::kMetricsReportingEnabled, false);
  EXPECT_FALSE(manager_client_->IsMetricsReportingEnabled());

  CobaltEnabledStateProvider* provider_false_case =
      manager_client_->GetEnabledStateProvider();
  ASSERT_THAT(provider_false_case, NotNull());
  EXPECT_FALSE(provider_false_case->IsReportingEnabled());
  EXPECT_EQ(provider_false_case->IsReportingEnabled(),
            manager_client_->IsMetricsReportingEnabled());

  // Case 2: Pref is true.
  prefs_.SetBoolean(metrics::prefs::kMetricsReportingEnabled, true);
  EXPECT_TRUE(manager_client_->IsMetricsReportingEnabled());

  CobaltEnabledStateProvider* provider_true_case =
      manager_client_->GetEnabledStateProvider();
  ASSERT_THAT(provider_true_case, NotNull());
  EXPECT_TRUE(provider_true_case->IsReportingEnabled());
  EXPECT_EQ(provider_true_case->IsReportingEnabled(),
            manager_client_->IsMetricsReportingEnabled());
}

TEST_F(CobaltMetricsServicesManagerClientTest,
       IsMetricsConsentGivenReflectsPref) {
  // IsMetricsConsentGiven now mirrors IsMetricsReportingEnabled.
  // Case 1: Pref is false.
  prefs_.SetBoolean(metrics::prefs::kMetricsReportingEnabled, false);
  EXPECT_FALSE(manager_client_->IsMetricsConsentGiven());

  CobaltEnabledStateProvider* provider_false_case =
      manager_client_->GetEnabledStateProvider();
  ASSERT_THAT(provider_false_case, NotNull());
  EXPECT_FALSE(provider_false_case->IsConsentGiven());
  EXPECT_EQ(provider_false_case->IsConsentGiven(),
            manager_client_->IsMetricsConsentGiven());

  // Case 2: Pref is true.
  prefs_.SetBoolean(metrics::prefs::kMetricsReportingEnabled, true);
  EXPECT_TRUE(manager_client_->IsMetricsConsentGiven());

  CobaltEnabledStateProvider* provider_true_case =
      manager_client_->GetEnabledStateProvider();
  ASSERT_THAT(provider_true_case, NotNull());
  EXPECT_TRUE(provider_true_case->IsConsentGiven());
  EXPECT_EQ(provider_true_case->IsConsentGiven(),
            manager_client_->IsMetricsConsentGiven());
}

TEST_F(CobaltMetricsServicesManagerClientTest,
       MetricsReportingIsEnabledByForceSwitch) {
  // Pref is false.
  prefs_.SetBoolean(metrics::prefs::kMetricsReportingEnabled, false);

  // Add the force-enable switch.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      metrics::switches::kForceEnableMetricsReporting);

  EXPECT_TRUE(manager_client_->IsMetricsReportingEnabled())
      << "Should be true due to command-line override.";
  EXPECT_TRUE(manager_client_->IsMetricsConsentGiven())
      << "Should be true due to command-line override.";

  CobaltEnabledStateProvider* provider =
      manager_client_->GetEnabledStateProvider();
  ASSERT_THAT(provider, NotNull());
  EXPECT_TRUE(provider->IsReportingEnabled());
  EXPECT_TRUE(provider->IsConsentGiven());

  // Clean up the switch for other tests.
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      metrics::switches::kForceEnableMetricsReporting);
  EXPECT_FALSE(manager_client_->IsMetricsReportingEnabled())
      << "Should be false again after removing override.";
}

TEST_F(CobaltMetricsServicesManagerClientTest,
       IsOffTheRecordSessionActiveReturnsFalse) {
  EXPECT_FALSE(manager_client_->IsOffTheRecordSessionActive());
}

TEST_F(CobaltMetricsServicesManagerClientTest, UpdateRunningServicesIsNoOp) {
  manager_client_->UpdateRunningServices(/*may_record=*/true,
                                         /*may_upload=*/true);
  manager_client_->UpdateRunningServices(false, false);
  SUCCEED();
  // Nothing to test except no crash.
}

TEST_F(CobaltMetricsServicesManagerClientTest,
       GetMetricsStateManagerCreatesAndReturnsInstance) {
  // Set a known state for prefs to ensure EnabledStateProvider behaves
  // predictably for MetricsStateManager creation.
  prefs_.SetBoolean(metrics::prefs::kMetricsReportingEnabled, true);

  ::metrics::MetricsStateManager* state_manager =
      manager_client_->GetMetricsStateManager();
  EXPECT_THAT(state_manager, NotNull());
  // Check that the state_manager reflects the enabled state.
  EXPECT_TRUE(state_manager->IsMetricsReportingEnabled());
}

TEST_F(CobaltMetricsServicesManagerClientTest,
       GetMetricsStateManagerReturnsSameInstanceOnSubsequentCalls) {
  ::metrics::MetricsStateManager* state_manager1 =
      manager_client_->GetMetricsStateManager();
  ASSERT_THAT(state_manager1, NotNull());
  ::metrics::MetricsStateManager* state_manager2 =
      manager_client_->GetMetricsStateManager();
  EXPECT_EQ(state_manager1, state_manager2);
}

TEST_F(CobaltMetricsServicesManagerClientTest,
       CreateMetricsServiceClientCreatesClient) {
  ASSERT_THAT(manager_client_->GetMetricsStateManager(), NotNull());

  std::unique_ptr<::metrics::MetricsServiceClient> metrics_service_client =
      manager_client_->CreateMetricsServiceClient();

  EXPECT_THAT(metrics_service_client, NotNull());
  EXPECT_THAT(
      static_cast<CobaltMetricsServiceClient*>(metrics_service_client.get()),
      NotNull());

  EXPECT_EQ(metrics_service_client.get(),
            manager_client_->metrics_service_client());
}

TEST_F(CobaltMetricsServicesManagerClientTest,
       CreateMetricsServiceClientCanBeCalledMultipleTimes) {
  std::unique_ptr<::metrics::MetricsServiceClient> client1 =
      manager_client_->CreateMetricsServiceClient();
  ASSERT_THAT(client1, NotNull());
  EXPECT_EQ(client1.get(), manager_client_->metrics_service_client());

  std::unique_ptr<::metrics::MetricsServiceClient> client2 =
      manager_client_->CreateMetricsServiceClient();
  ASSERT_THAT(client2, NotNull());
  EXPECT_NE(client1.get(), client2.get());
  EXPECT_EQ(client2.get(), manager_client_->metrics_service_client());
}

TEST_F(CobaltMetricsServicesManagerClientTest,
       CreateVariationsServiceReturnsNullAndDoesNotCrash) {
  EXPECT_THAT(manager_client_->CreateVariationsService(), IsNull());
}

TEST_F(CobaltMetricsServicesManagerClientTest,
       GetEnabledStateProviderReturnsProviderWithCorrectState) {
  // Case 1: Pref is false.
  prefs_.SetBoolean(metrics::prefs::kMetricsReportingEnabled, false);
  CobaltEnabledStateProvider* provider_false_case =
      manager_client_->GetEnabledStateProvider();
  ASSERT_THAT(provider_false_case, NotNull());
  EXPECT_FALSE(provider_false_case->IsConsentGiven());
  EXPECT_FALSE(provider_false_case->IsReportingEnabled());

  // Case 2: Pref is true.
  prefs_.SetBoolean(metrics::prefs::kMetricsReportingEnabled, true);
  CobaltEnabledStateProvider* provider_true_case =
      manager_client_->GetEnabledStateProvider();
  ASSERT_THAT(provider_true_case, NotNull());
  EXPECT_TRUE(provider_true_case->IsConsentGiven());
  EXPECT_TRUE(provider_true_case->IsReportingEnabled());

  // Check that multiple calls return the same provider instance for a given
  // manager_client.
  EXPECT_EQ(provider_true_case, manager_client_->GetEnabledStateProvider());
}

}  // namespace cobalt
