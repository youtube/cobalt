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

#include "cobalt/browser/h5vcc_metrics/histogram_fetcher.h"

#include "base/base64url.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/metrics_hashes.h"
#include "base/metrics/statistics_recorder.h"
#include "base/test/task_environment.h"
#include "cobalt/browser/metrics/cobalt_enabled_state_provider.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/test/test_metrics_service_client.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/cobalt_uma_event.pb.h"

namespace h5vcc_metrics {

namespace {
const char kTestHistogram[] = "Test.Histogram";
}  // namespace

class HistogramFetcherTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Register all metric-related prefs.
    metrics::MetricsService::RegisterPrefs(prefs_.registry());
    prefs_.registry()->RegisterBooleanPref(
        metrics::prefs::kMetricsReportingEnabled, false);

    // Set the state to enabled for the test.
    prefs_.SetBoolean(metrics::prefs::kMetricsReportingEnabled, true);
    // Set a valid client ID to satisfy the DCHECK.
    prefs_.SetString(metrics::prefs::kMetricsClientID,
                     service_client_.get_client_id());

    // Create a MetricsStateManager.
    enabled_state_provider_ =
        std::make_unique<cobalt::CobaltEnabledStateProvider>(&prefs_);
    state_manager_ = metrics::MetricsStateManager::Create(
        &prefs_, enabled_state_provider_.get(), std::wstring(),
        base::FilePath());
  }

  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple prefs_;
  metrics::TestMetricsServiceClient service_client_;
  std::unique_ptr<cobalt::CobaltEnabledStateProvider> enabled_state_provider_;
  std::unique_ptr<metrics::MetricsStateManager> state_manager_;
};

TEST_F(HistogramFetcherTest, FetchHistograms_CalculatesDeltaCorrectly) {
  HistogramFetcher fetcher;

  // stablish baseline and check for the histogram snapshot.
  fetcher.FetchHistograms(state_manager_.get(), &service_client_);
  UMA_HISTOGRAM_BOOLEAN(kTestHistogram, true);
  std::string returned_base64 =
      fetcher.FetchHistograms(state_manager_.get(), &service_client_);

  std::string decoded_proto;
  ASSERT_TRUE(base::Base64UrlDecode(
      returned_base64, base::Base64UrlDecodePolicy::REQUIRE_PADDING,
      &decoded_proto));

  cobalt::browser::metrics::CobaltUMAEvent uma_event;
  ASSERT_TRUE(uma_event.ParseFromString(decoded_proto));
  ASSERT_THAT(uma_event.histogram_event(), testing::SizeIs(1));
  EXPECT_EQ(uma_event.histogram_event(0).name_hash(),
            base::HashMetricName(kTestHistogram));

  // Add another sample and check for a second time.
  UMA_HISTOGRAM_BOOLEAN(kTestHistogram, false);
  returned_base64 =
      fetcher.FetchHistograms(state_manager_.get(), &service_client_);

  ASSERT_TRUE(base::Base64UrlDecode(
      returned_base64, base::Base64UrlDecodePolicy::REQUIRE_PADDING,
      &decoded_proto));
  ASSERT_TRUE(uma_event.ParseFromString(decoded_proto));
  ASSERT_THAT(uma_event.histogram_event(), testing::SizeIs(1));
  EXPECT_EQ(uma_event.histogram_event(0).name_hash(),
            base::HashMetricName(kTestHistogram));
}

}  // namespace h5vcc_metrics
