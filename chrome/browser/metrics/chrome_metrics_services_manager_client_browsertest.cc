// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_metrics_services_manager_client.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/settings/metrics_reporting_level_controller.h"
#include "chrome/browser/ash/settings/stats_reporting_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/metrics/enabled_state_provider.h"
#include "components/metrics/metrics_features.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_reporting_choice_service.h"
#include "components/metrics/metrics_reporting_level.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace {

class TestMetricsServiceAccessor : public metrics::MetricsServiceAccessor {
 public:
  static void SetForceIsMetricsReportingEnabledPrefLookup(bool value) {
    metrics::MetricsServiceAccessor::
        SetForceIsMetricsReportingEnabledPrefLookup(value);
  }
};

}  // namespace

class ChromeMetricsServicesManagerClientBrowserTest
    : public InProcessBrowserTest {
 public:
  ChromeMetricsServicesManagerClientBrowserTest() = default;
  ~ChromeMetricsServicesManagerClientBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    feature_list_.InitAndEnableFeature(
        metrics::features::kRestructureMetricsConsentSettings);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ChromeMetricsServicesManagerClientBrowserTest,
                       CrosMetricsReportingLevelChange_MigrationDone) {
  TestMetricsServiceAccessor::SetForceIsMetricsReportingEnabledPrefLookup(true);
  PrefService* local_state = g_browser_process->local_state();

  // Set the migration preference to true.
  local_state->SetBoolean(metrics::prefs::kMetricsReportingMigrationDone, true);
  metrics::MetricsReportingChoiceService::ClearCachedFeatureStateForTesting();
  EXPECT_TRUE(metrics::MetricsReportingChoiceService::
                  ShouldUseMetricsConsentRestructure(local_state));

  // Initialize both controllers to disabled/none.
  ash::StatsReportingController::Get()->SetEnabled(browser()->profile(), false);
  ash::MetricsReportingLevelController::Get()->SetLevel(
      browser()->profile(), metrics::MetricsReportingLevel::kNone);
  content::RunAllTasksUntilIdle();

  auto* manager = g_browser_process->GetMetricsServicesManager();

  EXPECT_FALSE(manager->IsMetricsReportingEnabled());

  // 1. Verify that changing the legacy Stats controller has no effect.
  ash::StatsReportingController::Get()->SetEnabled(browser()->profile(), true);
  content::RunAllTasksUntilIdle();
  EXPECT_FALSE(manager->IsMetricsReportingEnabled());

  // 2. Verify that changing the new Level controller works.
  ash::MetricsReportingLevelController::Get()->SetLevel(
      browser()->profile(), metrics::MetricsReportingLevel::kBasic);
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(local_state->GetInteger(metrics::prefs::kMetricsReportingLevel),
            static_cast<int>(metrics::MetricsReportingLevel::kBasic));
  EXPECT_TRUE(manager->IsMetricsReportingEnabled());

  // 3. Verify that resetting the legacy Stats controller back to false
  // doesn't disable UMA.
  ash::StatsReportingController::Get()->SetEnabled(browser()->profile(), false);
  content::RunAllTasksUntilIdle();
  EXPECT_TRUE(manager->IsMetricsReportingEnabled());

  // 4. Verify that disabling the new Level controller disables UMA.
  ash::MetricsReportingLevelController::Get()->SetLevel(
      browser()->profile(), metrics::MetricsReportingLevel::kNone);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(local_state->GetInteger(metrics::prefs::kMetricsReportingLevel),
            static_cast<int>(metrics::MetricsReportingLevel::kNone));
  EXPECT_FALSE(manager->IsMetricsReportingEnabled());

  TestMetricsServiceAccessor::SetForceIsMetricsReportingEnabledPrefLookup(
      false);
}

IN_PROC_BROWSER_TEST_F(ChromeMetricsServicesManagerClientBrowserTest,
                       CrosMetricsReportingLevelChange_MigrationNotDone) {
  TestMetricsServiceAccessor::SetForceIsMetricsReportingEnabledPrefLookup(true);
  PrefService* local_state = g_browser_process->local_state();

  // Set the migration preference to false.
  local_state->SetBoolean(metrics::prefs::kMetricsReportingMigrationDone,
                          false);
  metrics::MetricsReportingChoiceService::ClearCachedFeatureStateForTesting();
  EXPECT_FALSE(metrics::MetricsReportingChoiceService::
                   ShouldUseMetricsConsentRestructure(local_state));

  // Initialize both controllers to disabled/none.
  ash::StatsReportingController::Get()->SetEnabled(browser()->profile(), false);
  ash::MetricsReportingLevelController::Get()->SetLevel(
      browser()->profile(), metrics::MetricsReportingLevel::kNone);
  content::RunAllTasksUntilIdle();

  auto* manager = g_browser_process->GetMetricsServicesManager();

  EXPECT_FALSE(manager->IsMetricsReportingEnabled());

  // 1. Verify that changing the new Level controller has no effect.
  ash::MetricsReportingLevelController::Get()->SetLevel(
      browser()->profile(), metrics::MetricsReportingLevel::kBasic);
  content::RunAllTasksUntilIdle();
  EXPECT_FALSE(manager->IsMetricsReportingEnabled());

  // 2. Verify that changing the legacy Stats controller works.
  ash::StatsReportingController::Get()->SetEnabled(browser()->profile(), true);
  content::RunAllTasksUntilIdle();
  EXPECT_TRUE(manager->IsMetricsReportingEnabled());

  // 3. Verify that resetting the Level controller back to None doesn't
  // disable UMA.
  ash::MetricsReportingLevelController::Get()->SetLevel(
      browser()->profile(), metrics::MetricsReportingLevel::kNone);
  content::RunAllTasksUntilIdle();
  EXPECT_TRUE(manager->IsMetricsReportingEnabled());

  // 4. Verify that disabling the legacy Stats controller disables UMA.
  ash::StatsReportingController::Get()->SetEnabled(browser()->profile(), false);
  content::RunAllTasksUntilIdle();
  EXPECT_FALSE(manager->IsMetricsReportingEnabled());

  TestMetricsServiceAccessor::SetForceIsMetricsReportingEnabledPrefLookup(
      false);
}
