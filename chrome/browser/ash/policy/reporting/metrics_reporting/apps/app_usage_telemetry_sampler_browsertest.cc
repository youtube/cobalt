// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <tuple>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/string_piece_forward.h"
#include "base/synchronization/lock.h"
#include "base/test/scoped_feature_list.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics_utils.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/policy/affiliation/affiliation_mixin.h"
#include "chrome/browser/ash/policy/affiliation/affiliation_test_helper.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/metric_reporting_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/browser/chromeos/reporting/metric_default_utils.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom-shared.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/user_display_mode.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/dbus/missive/missive_client_test_observer.h"
#include "components/app_constants/constants.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/services/app_service/public/protos/app_types.pb.h"
#include "components/sync/test/test_sync_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "content/public/test/test_utils.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-shared.h"
#include "url/gurl.h"

using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Ge;
using ::testing::Le;
using ::testing::StrEq;
using ::testing::UnorderedPointwise;

namespace reporting {
namespace {

// Test DM token used to associate reported events.
constexpr char kDMToken[] = "token";

// Standalone webapp start URL.
constexpr char kWebAppUrl[] = "https://test.example.com";

// App usage UKM entry name.
constexpr char kAppUsageUKMEntryName[] = "ChromeOSApp.UsageTime";

// App usage collection interval.
constexpr base::TimeDelta kAppUsageCollectionInterval = base::Minutes(5);

// UKM app usage reporting interval.
constexpr base::TimeDelta kAppUsageUKMReportingInterval = base::Hours(2);

// Additional webapp usage buffer period before the browser is actually closed.
// Used when validating reported app usage data.
constexpr base::TimeDelta kWebAppUsageBufferPeriod = base::Seconds(5);

MATCHER(ContainsAppId,
        "Matches app id in app usage proto with the expected one") {
  return std::get<0>(arg).app_id() == std::get<1>(arg);
}

// Mock clock that supplies a mock time and can be advanced for testing
// purposes. Needed to simulate usage, and trigger telemetry collection and
// reporting. Only applicable for browser tests where a `TaskEnvironment` is
// unavailable.
class MockClock {
 public:
  // Returns a MockClock which will not be deleted. Should be called once while
  // single-threaded to initialize ScopedTimeClockOverrides to avoid threading
  // issues.
  static MockClock& Get() {
    static base::NoDestructor<MockClock> mock_clock;
    return *mock_clock;
  }

  MockClock(const MockClock&) = delete;
  MockClock& operator=(const MockClock&) = delete;
  ~MockClock() = default;

  // Advance clock by the specified duration.
  void Advance(const base::TimeDelta& duration) {
    base::AutoLock lock(lock_);
    offset_ += duration;
  }

 private:
  friend base::NoDestructor<MockClock>;

  static base::Time MockedNow() {
    return base::subtle::TimeNowIgnoringOverride() + Get().Offset();
  }

  static base::TimeTicks MockedTicksNow() {
    return base::subtle::TimeTicksNowIgnoringOverride() + Get().Offset();
  }

  MockClock()
      : time_override_(std::make_unique<base::subtle::ScopedTimeClockOverrides>(
            &MockClock::MockedNow,
            &MockClock::MockedTicksNow,
            nullptr)) {}

  // Returns the offset duration.
  const base::TimeDelta& Offset() {
    base::AutoLock lock(lock_);
    return offset_;
  }

  const std::unique_ptr<base::subtle::ScopedTimeClockOverrides> time_override_;

  // A lock is necessary because `MockedNow` and `MockedTicksNow` can be
  // accessed by components from different threads when they retrieve the
  // current time.
  base::Lock lock_;
  base::TimeDelta offset_ GUARDED_BY(lock_);
};

// Assert app usage telemetry data in a record with relevant DM token and
// returns the underlying `MetricData` object.
const MetricData AssertAppUsageTelemetryData(Priority priority,
                                             const Record& record) {
  EXPECT_THAT(priority, Eq(Priority::MANUAL_BATCH));
  EXPECT_THAT(record.destination(), Eq(Destination::TELEMETRY_METRIC));

  MetricData record_data;
  EXPECT_TRUE(record_data.ParseFromString(record.data()));
  EXPECT_TRUE(record_data.has_timestamp_ms());
  EXPECT_TRUE(record.has_dm_token());
  EXPECT_THAT(record.dm_token(), StrEq(kDMToken));
  return record_data;
}

// Returns true if the record includes app usage telemetry. False otherwise.
bool IsAppUsageTelemetry(const Record& record) {
  MetricData record_data;
  return record_data.ParseFromString(record.data()) &&
         record_data.has_telemetry_data() &&
         record_data.telemetry_data().has_app_telemetry() &&
         record_data.telemetry_data().app_telemetry().has_app_usage_data();
}

// Browser test that validates app usage telemetry reported by the
// `AppUsageTelemetrySampler`. Inheriting from `DevicePolicyCrosBrowserTest`
// enables use of `AffiliationMixin` for setting up profile/device affiliation.
// Only available in Ash.
class AppUsageTelemetrySamplerBrowserTest
    : public ::policy::DevicePolicyCrosBrowserTest {
 protected:
  AppUsageTelemetrySamplerBrowserTest() {
    // Initialize the MockClock.
    MockClock::Get();
    crypto_home_mixin_.MarkUserAsExisting(affiliation_mixin_.account_id());
    scoped_feature_list_.InitAndEnableFeature(kEnableAppMetricsReporting);
    ::policy::SetDMTokenForTesting(
        ::policy::DMToken::CreateValidTokenForTesting(kDMToken));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ::policy::AffiliationTestHelper::AppendCommandLineSwitchesForLoginManager(
        command_line);
    ::policy::DevicePolicyCrosBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    ::policy::DevicePolicyCrosBrowserTest::SetUpOnMainThread();
    SetPolicyEnabled(true);
    if (::content::IsPreTest()) {
      // Preliminary setup - set up affiliated user.
      ::policy::AffiliationTestHelper::PreLoginUser(
          affiliation_mixin_.account_id());
      return;
    }

    // Login as affiliated user otherwise and set up test environment.
    ::policy::AffiliationTestHelper::LoginUser(affiliation_mixin_.account_id());
    ::web_app::test::UninstallAllWebApps(profile());
    test_ukm_recorder_ = std::make_unique<::ukm::TestAutoSetUkmRecorder>();
  }

  void SetUpInProcessBrowserTestFixture() override {
    ::policy::DevicePolicyCrosBrowserTest::SetUpInProcessBrowserTestFixture();
    create_sync_service_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &AppUsageTelemetrySamplerBrowserTest::SetUpSyncService,
                base::Unretained(this)));
  }

  void SetUpSyncService(::content::BrowserContext* context) {
    SyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        context, base::BindRepeating([](::content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
          return std::make_unique<::syncer::TestSyncService>();
        }));
  }

  // Helper that installs a standalone webapp with the specified start url.
  ::web_app::AppId InstallStandaloneWebApp(const GURL& start_url) {
    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app_info->start_url = start_url;
    web_app_info->scope = start_url.GetWithoutFilename();
    web_app_info->display_mode = ::blink::mojom::DisplayMode::kStandalone;
    web_app_info->user_display_mode =
        ::web_app::mojom::UserDisplayMode::kStandalone;
    return ::web_app::test::InstallWebApp(profile(), std::move(web_app_info));
  }

  // Helper that simulates app usage for the specified app and usage duration.
  void SimulateAppUsage(const ::web_app::AppId& app_id,
                        const base::TimeDelta& running_time) {
    // Launch web app and simulate web app usage before closing the browser
    // window to prevent further usage tracking.
    Browser* const app_browser =
        ::web_app::LaunchWebAppBrowser(profile(), app_id);
    MockClock::Get().Advance(running_time);
    ::web_app::CloseAndWait(app_browser);

    // Trigger usage telemetry collection by advancing the clock. Wait before
    // returning to ensure usage data gets persisted in the user pref store.
    MockClock::Get().Advance(kAppUsageCollectionInterval);
    ::content::RunAllTasksUntilIdle();
  }

  void VerifyAppUsage(const AppUsageData::AppUsage& app_usage,
                      const ::web_app::AppId& app_id,
                      const base::TimeDelta& running_time) {
    EXPECT_TRUE(app_usage.has_app_instance_id());
    EXPECT_THAT(app_usage.app_type(),
                Eq(::apps::ApplicationType::APPLICATION_TYPE_WEB));

    // There is some minor usage (usually in milliseconds) as we attempt to
    // close the browser and before it is actually closed, so we account for
    // that below as we validate reported usage.
    const auto& max_expected_usage = running_time + kWebAppUsageBufferPeriod;
    EXPECT_THAT(app_usage.running_time_ms(),
                AllOf(Ge(running_time.InMilliseconds()),
                      Le(max_expected_usage.InMilliseconds())));

    // Also verify app usage data is reset if not yet cleared from the pref
    // store because this data was reported.
    const auto& app_usage_dict =
        profile()->GetPrefs()->GetDict(::apps::kAppUsageTime);
    if (app_usage_dict.contains(app_usage.app_instance_id())) {
      EXPECT_THAT(
          *app_usage_dict.FindDictByDottedPath(app_usage.app_instance_id())
               ->FindString(::apps::kReportingUsageTimeDurationKey),
          StrEq("0"));
    }
  }

  const AppUsageData::AppUsage& GetAppUsageInfoForApp(
      const AppUsageData& app_usage_data,
      const ::web_app::AppId& app_id) {
    for (const auto& app_usage : app_usage_data.app_usage()) {
      if (app_usage.app_id() == app_id) {
        return app_usage;
      }
    }
    NOTREACHED_NORETURN() << "App usage info missing for app with id: "
                          << app_id;
  }

  void VerifyWebAppUsageUKM(base::StringPiece instance_id,
                            const base::TimeDelta& running_time) {
    const auto entries =
        test_ukm_recorder_->GetEntriesByName(kAppUsageUKMEntryName);
    int usage_time = 0;
    for (const auto* entry : entries) {
      const ::ukm::UkmSource* source =
          test_ukm_recorder_->GetSourceForSourceId(entry->source_id);
      if (!source || source->url() != GURL(kWebAppUrl)) {
        continue;
      }
      usage_time += *(test_ukm_recorder_->GetEntryMetric(entry, "Duration"));
      test_ukm_recorder_->ExpectEntryMetric(entry, "UserDeviceMatrix", 0);
      test_ukm_recorder_->ExpectEntryMetric(entry, "AppType",
                                            (int)::apps::AppTypeName::kWeb);
    }

    // There is some minor usage (usually in milliseconds) as we attempt to
    // close the browser and before it is actually closed, so we account for
    // that below as we validate app usage reported to UKM.
    const auto& max_expected_usage = running_time + kWebAppUsageBufferPeriod;
    EXPECT_THAT(usage_time, AllOf(Ge(running_time.InMilliseconds()),
                                  Le(max_expected_usage.InMilliseconds())));

    // Also verify app usage data is reset if not yet cleared from the pref
    // store because this data was already reported to UKM.
    const auto& app_usage_dict =
        profile()->GetPrefs()->GetDict(::apps::kAppUsageTime);
    if (app_usage_dict.contains(instance_id)) {
      EXPECT_THAT(*app_usage_dict.FindDictByDottedPath(instance_id)
                       ->FindString(::apps::kUsageTimeDurationKey),
                  StrEq("0"));
    }
  }

  void SetPolicyEnabled(bool is_enabled) {
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        ::ash::kReportDeviceAppInfo, is_enabled);
  }

  Profile* profile() const {
    return ::ash::ProfileHelper::Get()->GetProfileByAccountId(
        affiliation_mixin_.account_id());
  }

  ::syncer::TestSyncService* sync_service() const {
    return static_cast<::syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(profile()));
  }

  ::policy::DevicePolicyCrosTestHelper test_helper_;
  ::policy::AffiliationMixin affiliation_mixin_{&mixin_host_, &test_helper_};
  ::ash::CryptohomeMixin crypto_home_mixin_{&mixin_host_};

  base::test::ScopedFeatureList scoped_feature_list_;
  ::ash::ScopedTestingCrosSettings scoped_testing_cros_settings_;

  base::CallbackListSubscription create_sync_service_subscription_;
  std::unique_ptr<::ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
};

IN_PROC_BROWSER_TEST_F(AppUsageTelemetrySamplerBrowserTest,
                       PRE_ReportUsageData) {
  // Dummy case that sets up the affiliated user through SetUpOnMainThread
  // PRE-condition.
}

IN_PROC_BROWSER_TEST_F(AppUsageTelemetrySamplerBrowserTest, ReportUsageData) {
  ASSERT_TRUE(base::FeatureList::IsEnabled(kEnableAppMetricsReporting));

  // Install webapp and simulate its usage.
  const auto& app_id = InstallStandaloneWebApp(GURL(kWebAppUrl));
  static constexpr base::TimeDelta kAppUsageDuration = base::Minutes(2);
  ::chromeos::MissiveClientTestObserver missive_observer(
      base::BindRepeating(&IsAppUsageTelemetry));
  SimulateAppUsage(app_id, kAppUsageDuration);

  // Force telemetry collection by advancing the timer and verify data that is
  // being enqueued via ERP.
  MockClock::Get().Advance(
      metrics::kDefaultDeviceActivityHeartbeatCollectionRate);
  const auto [priority, record] = missive_observer.GetNextEnqueuedRecord();
  const auto& metric_data = AssertAppUsageTelemetryData(priority, record);

  // Data reported includes usage from the web app + the native Chrome
  // component application since these leverage the browser.
  const auto& app_usage_data =
      metric_data.telemetry_data().app_telemetry().app_usage_data();
  ASSERT_THAT(app_usage_data.app_usage().size(), Eq(2));
  ASSERT_THAT(
      app_usage_data.app_usage(),
      UnorderedPointwise(
          ContainsAppId(),
          {app_id, static_cast<std::string>(::app_constants::kChromeAppId)}));
  const auto& app_usage = GetAppUsageInfoForApp(app_usage_data, app_id);
  VerifyAppUsage(app_usage, app_id, kAppUsageDuration);

  // Trigger upload to UKM by advancing the timer.
  MockClock::Get().Advance(kAppUsageUKMReportingInterval);
  ::content::RunAllTasksUntilIdle();
  VerifyWebAppUsageUKM(app_usage.app_instance_id(), kAppUsageDuration);

  // Advance the timer and verify data is cleared by the next upload cycle.
  MockClock::Get().Advance(kAppUsageUKMReportingInterval);
  ::content::RunAllTasksUntilIdle();
  ASSERT_FALSE(profile()
                   ->GetPrefs()
                   ->GetDict(::apps::kAppUsageTime)
                   .contains(app_usage.app_instance_id()));
}

IN_PROC_BROWSER_TEST_F(AppUsageTelemetrySamplerBrowserTest,
                       PRE_ReportUsageDataWhenSyncDisabled) {
  // Dummy case that sets up the affiliated user through SetUpOnMainThread
  // PRE-condition.
}

IN_PROC_BROWSER_TEST_F(AppUsageTelemetrySamplerBrowserTest,
                       ReportUsageDataWhenSyncDisabled) {
  ASSERT_TRUE(base::FeatureList::IsEnabled(kEnableAppMetricsReporting));
  sync_service()->SetDisableReasons(
      ::syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY);

  // Install web app and simulate its usage.
  const auto& app_id = InstallStandaloneWebApp(GURL(kWebAppUrl));
  static constexpr base::TimeDelta kAppUsageDuration = base::Minutes(2);
  ::chromeos::MissiveClientTestObserver missive_observer(
      base::BindRepeating(&IsAppUsageTelemetry));
  SimulateAppUsage(app_id, kAppUsageDuration);

  // Force telemetry collection by advancing the timer and verify data that is
  // being enqueued via ERP.
  MockClock::Get().Advance(
      metrics::kDefaultDeviceActivityHeartbeatCollectionRate);
  const auto [priority, record] = missive_observer.GetNextEnqueuedRecord();
  const auto& metric_data = AssertAppUsageTelemetryData(priority, record);

  // Data reported includes usage from the web app + the native Chrome
  // component application since these leverage the browser.
  const auto& app_usage_data =
      metric_data.telemetry_data().app_telemetry().app_usage_data();
  ASSERT_THAT(app_usage_data.app_usage().size(), Eq(2));
  ASSERT_THAT(
      app_usage_data.app_usage(),
      UnorderedPointwise(
          ContainsAppId(),
          {app_id, static_cast<std::string>(::app_constants::kChromeAppId)}));
  const auto& app_usage = GetAppUsageInfoForApp(app_usage_data, app_id);
  VerifyAppUsage(app_usage, app_id, kAppUsageDuration);

  // Advance timer and verify no data is reported to UKM.
  MockClock::Get().Advance(kAppUsageUKMReportingInterval);
  ::content::RunAllTasksUntilIdle();
  ASSERT_THAT(
      test_ukm_recorder_->GetEntriesByName(kAppUsageUKMEntryName).size(),
      Eq(0uL));
}

IN_PROC_BROWSER_TEST_F(AppUsageTelemetrySamplerBrowserTest,
                       PRE_ReportUsageDataWhenPolicyDisabled) {
  // Dummy case that sets up the affiliated user through SetUpOnMainThread
  // PRE-condition.
}

IN_PROC_BROWSER_TEST_F(AppUsageTelemetrySamplerBrowserTest,
                       ReportUsageDataWhenPolicyDisabled) {
  ASSERT_TRUE(base::FeatureList::IsEnabled(kEnableAppMetricsReporting));
  SetPolicyEnabled(false);

  // Install web app and simulate its usage.
  const auto& app_id = InstallStandaloneWebApp(GURL(kWebAppUrl));
  static constexpr base::TimeDelta kAppUsageDuration = base::Minutes(2);
  ::chromeos::MissiveClientTestObserver missive_observer(
      base::BindRepeating(&IsAppUsageTelemetry));
  SimulateAppUsage(app_id, kAppUsageDuration);

  // Force telemetry collection by advancing the timer and verify no data is
  // being enqueued.
  MockClock::Get().Advance(
      metrics::kDefaultDeviceActivityHeartbeatCollectionRate);
  ::content::RunAllTasksUntilIdle();
  ASSERT_FALSE(missive_observer.HasNewEnqueuedRecords());
}

}  // namespace
}  // namespace reporting
