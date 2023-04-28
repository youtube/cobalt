// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/stability_metrics_provider.h"

#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace metrics {

class StabilityMetricsProviderTest : public testing::Test {
 public:
  StabilityMetricsProviderTest() {
    StabilityMetricsProvider::RegisterPrefs(prefs_.registry());
  }

  ~StabilityMetricsProviderTest() override {}

 protected:
  TestingPrefServiceSimple prefs_;

 private:
  DISALLOW_COPY_AND_ASSIGN(StabilityMetricsProviderTest);
};

TEST_F(StabilityMetricsProviderTest, ProvideStabilityMetrics) {
  StabilityMetricsProvider stability_provider(&prefs_);
  MetricsProvider* provider = &stability_provider;
  SystemProfileProto system_profile;
  provider->ProvideStabilityMetrics(&system_profile);

  const SystemProfileProto_Stability& stability = system_profile.stability();
  // Initial log metrics: only expected if non-zero.
  EXPECT_FALSE(stability.has_launch_count());
  EXPECT_FALSE(stability.has_crash_count());
  EXPECT_FALSE(stability.has_incomplete_shutdown_count());
  EXPECT_FALSE(stability.has_breakpad_registration_success_count());
  EXPECT_FALSE(stability.has_breakpad_registration_failure_count());
  EXPECT_FALSE(stability.has_debugger_present_count());
  EXPECT_FALSE(stability.has_debugger_not_present_count());
}

TEST_F(StabilityMetricsProviderTest, RecordStabilityMetrics) {
  {
    StabilityMetricsProvider recorder(&prefs_);
    recorder.LogLaunch();
    recorder.LogCrash(base::Time());
    recorder.MarkSessionEndCompleted(false);
    recorder.CheckLastSessionEndCompleted();
    recorder.RecordBreakpadRegistration(true);
    recorder.RecordBreakpadRegistration(false);
    recorder.RecordBreakpadHasDebugger(true);
    recorder.RecordBreakpadHasDebugger(false);
  }

  {
    StabilityMetricsProvider stability_provider(&prefs_);
    MetricsProvider* provider = &stability_provider;
    SystemProfileProto system_profile;
    provider->ProvideStabilityMetrics(&system_profile);

    const SystemProfileProto_Stability& stability = system_profile.stability();
    // Initial log metrics: only expected if non-zero.
    EXPECT_EQ(1, stability.launch_count());
    EXPECT_EQ(1, stability.crash_count());
    EXPECT_EQ(1, stability.incomplete_shutdown_count());
    EXPECT_EQ(1, stability.breakpad_registration_success_count());
    EXPECT_EQ(1, stability.breakpad_registration_failure_count());
    EXPECT_EQ(1, stability.debugger_present_count());
    EXPECT_EQ(1, stability.debugger_not_present_count());
  }
}

#if defined(OS_WIN)
namespace {

class TestingStabilityMetricsProvider : public StabilityMetricsProvider {
 public:
  TestingStabilityMetricsProvider(PrefService* local_state,
                                  base::Time unclean_session_time)
      : StabilityMetricsProvider(local_state),
        unclean_session_time_(unclean_session_time) {}

  bool IsUncleanSystemSession(base::Time last_live_timestamp) override {
    return last_live_timestamp == unclean_session_time_;
  }

 private:
  const base::Time unclean_session_time_;
};

}  // namespace

TEST_F(StabilityMetricsProviderTest, RecordSystemCrashMetrics) {
  {
    base::Time unclean_time = base::Time::Now();
    TestingStabilityMetricsProvider recorder(&prefs_, unclean_time);

    // Any crash with a last_live_timestamp equal to unclean_time will
    // be logged as a system crash as per the implementation of
    // TestingStabilityMetricsProvider, so this will log a system crash.
    recorder.LogCrash(unclean_time);

    // Record a crash with no system crash.
    recorder.LogCrash(unclean_time - base::TimeDelta::FromMinutes(1));
  }

  {
    StabilityMetricsProvider stability_provider(&prefs_);
    MetricsProvider* provider = &stability_provider;
    SystemProfileProto system_profile;

    base::HistogramTester histogram_tester;

    provider->ProvideStabilityMetrics(&system_profile);

    const SystemProfileProto_Stability& stability = system_profile.stability();
    // Two crashes, one system crash.
    EXPECT_EQ(2, stability.crash_count());

    histogram_tester.ExpectTotalCount("Stability.Internals.SystemCrashCount",
                                      1);
  }
}

#endif

}  // namespace metrics
