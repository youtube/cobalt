// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "base/containers/contains.h"
#include "chrome/browser/page_load_metrics/integration_tests/metric_integration_test.h"
#include "chrome/browser/preloading/prerender/prerender_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_load_metrics/browser/observers/core/uma_page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/observers/prerender_page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "content/public/browser/prerender_trigger_type.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/features.h"

using PrerenderPageLoad = ukm::builders::PrerenderPageLoad;
using PageLoad = ukm::builders::PageLoad;

namespace {
const char kResponseWithNoStore[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html; charset=utf-8\r\n"
    "Cache-Control: no-store\r\n"
    "\r\n"
    "The server speaks HTTP!";
}

class PrerenderPageLoadMetricsObserverBrowserTest
    : public MetricIntegrationTest {
 public:
  PrerenderPageLoadMetricsObserverBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &PrerenderPageLoadMetricsObserverBrowserTest::web_contents,
            base::Unretained(this))) {
    // TODO(crbug.com/1239281): Remove this once kPrerender2MainFrameNavigation
    // is enabled by default.
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kPrerender2MainFrameNavigation);
  }
  ~PrerenderPageLoadMetricsObserverBrowserTest() override = default;

  void SetUp() override {
    prerender_helper_.SetUp(embedded_test_server());
    MetricIntegrationTest::SetUp();
  }

  // Similar to UkmRecorder::GetMergedEntriesByName(), but returned map is keyed
  // by source URL.
  std::map<GURL, ukm::mojom::UkmEntryPtr> GetMergedUkmEntries(
      const std::string& entry_name) {
    auto entries =
        ukm_recorder().GetMergedEntriesByName(PrerenderPageLoad::kEntryName);
    std::map<GURL, ukm::mojom::UkmEntryPtr> result;
    for (auto& kv : entries) {
      const ukm::mojom::UkmEntry* entry = kv.second.get();
      const ukm::UkmSource* source =
          ukm_recorder().GetSourceForSourceId(entry->source_id);
      EXPECT_TRUE(source);
      EXPECT_TRUE(source->url().is_valid());
      result.emplace(source->url(), std::move(kv.second));
    }
    return result;
  }

 protected:
  void CheckFirstPaintMetrics(
      content::PrerenderTriggerType trigger_type =
          content::PrerenderTriggerType::kSpeculationRule,
      const std::string& embedder_suffix = "") {
    histogram_tester().ExpectBucketCount(
        internal::kPageLoadPrerenderObserverEvent,
        internal::PageLoadPrerenderObserverEvent::kOnFirstPaintInPage, 1);
    histogram_tester().ExpectBucketCount(
        "PageLoad.Internal.Prerender2.ForegroundCheckResult.FirstPaint",
        internal::PageLoadPrerenderForegroundCheckResult::kPassed, 1);

    // FirstPaint should be recorded in the prerender PageLoad, not in the
    // regular PageLoad.
    histogram_tester().ExpectTotalCount(
        prerender_helper_.GenerateHistogramName(
            internal::kHistogramPrerenderActivationToFirstPaint, trigger_type,
            embedder_suffix),
        1);
    histogram_tester().ExpectTotalCount(internal::kHistogramFirstPaint, 0);
  }

  void CheckFirstContentfulPaintMetrics(
      content::PrerenderTriggerType trigger_type =
          content::PrerenderTriggerType::kSpeculationRule,
      const std::string& embedder_suffix = "") {
    histogram_tester().ExpectBucketCount(
        internal::kPageLoadPrerenderObserverEvent,
        internal::PageLoadPrerenderObserverEvent::kOnFirstContentfulPaintInPage,
        1);
    histogram_tester().ExpectBucketCount(
        "PageLoad.Internal.Prerender2.ForegroundCheckResult."
        "FirstContentfulPaint",
        internal::PageLoadPrerenderForegroundCheckResult::kPassed, 1);

    // FirstContentfulPaint should be recorded in the prerender PageLoad, not in
    // the regular PageLoad.
    histogram_tester().ExpectTotalCount(
        prerender_helper_.GenerateHistogramName(
            internal::kHistogramPrerenderActivationToFirstContentfulPaint,
            trigger_type, embedder_suffix),
        1);
    histogram_tester().ExpectTotalCount(
        internal::kHistogramFirstContentfulPaint, 0);
  }

  void CheckFirstInputDelayMetrics(
      content::PrerenderTriggerType trigger_type =
          content::PrerenderTriggerType::kSpeculationRule,
      const std::string& embedder_suffix = "") {
    histogram_tester().ExpectBucketCount(
        internal::kPageLoadPrerenderObserverEvent,
        internal::PageLoadPrerenderObserverEvent::kOnFirstInputInPage, 1);
    histogram_tester().ExpectBucketCount(
        "PageLoad.Internal.Prerender2.ForegroundCheckResult.FirstInputDelay",
        internal::PageLoadPrerenderForegroundCheckResult::kPassed, 1);

    // FirstInputDelay should be recorded in the prerender PageLoad, not in the
    // regular PageLoad.
    histogram_tester().ExpectTotalCount(
        prerender_helper_.GenerateHistogramName(
            internal::kHistogramPrerenderFirstInputDelay4, trigger_type,
            embedder_suffix),
        1);
    histogram_tester().ExpectTotalCount(internal::kHistogramFirstInputDelay, 0);
  }

  void CheckLargestContentfulPaintMetrics(
      content::PrerenderTriggerType trigger_type =
          content::PrerenderTriggerType::kSpeculationRule,
      const std::string& embedder_suffix = "") {
    histogram_tester().ExpectBucketCount(
        internal::kPageLoadPrerenderObserverEvent,
        internal::PageLoadPrerenderObserverEvent::kOnComplete, 1);
    histogram_tester().ExpectBucketCount(
        internal::kPageLoadPrerenderObserverEvent,
        internal::PageLoadPrerenderObserverEvent::kRecordSessionEndHistograms,
        1);
    histogram_tester().ExpectBucketCount(
        "PageLoad.Internal.Prerender2.ForegroundCheckResult."
        "LargestContentfulPaint",
        internal::PageLoadPrerenderForegroundCheckResult::kPassed, 1);

    // LargestContentfulPaint should be recorded in the prerender PageLoad, not
    // in the regular PageLoad.
    histogram_tester().ExpectTotalCount(
        prerender_helper_.GenerateHistogramName(
            internal::kHistogramPrerenderActivationToLargestContentfulPaint2,
            trigger_type, embedder_suffix),
        1);
    histogram_tester().ExpectTotalCount(
        internal::kHistogramLargestContentfulPaint, 0);
  }

  void CheckResponsivenessMetrics(const GURL& url) {
    histogram_tester().ExpectBucketCount(
        internal::kPageLoadPrerenderObserverEvent,
        internal::PageLoadPrerenderObserverEvent::
            kRecordNormalizedResponsivenessMetrics,
        1);

    std::vector<std::string> ukm_list = {
        "InteractiveTiming.WorstUserInteractionLatency.MaxEventDuration",
        "InteractiveTiming.AverageUserInteractionLatencyOverBudget."
        "MaxEventDuration",
        "InteractiveTiming.UserInteractionLatency.HighPercentile2."
        "MaxEventDuration",
        "InteractiveTiming.NumInteractions"};

    for (auto& ukm : ukm_list) {
      int count = 0;
      for (auto* entry :
           ukm_recorder().GetEntriesByName(PrerenderPageLoad::kEntryName)) {
        auto* source = ukm_recorder().GetSourceForSourceId(entry->source_id);
        if (!source)
          continue;
        if (source->url() != url)
          continue;
        if (!ukm_recorder().EntryHasMetric(entry, ukm.c_str()))
          continue;
        count++;
      }
      EXPECT_EQ(count, 1);
    }

    std::vector<std::string> uma_list = {
        internal::
            kHistogramPrerenderAverageUserInteractionLatencyOverBudgetMaxEventDuration,
        internal::kHistogramPrerenderNumInteractions,
        internal::
            kHistogramPrerenderUserInteractionLatencyHighPercentile2MaxEventDuration,
        internal::
            kHistogramPrerenderWorstUserInteractionLatencyMaxEventDuration};

    for (auto& uma : uma_list) {
      histogram_tester().ExpectTotalCount(uma, 1);
    }
  }

  content::test::PrerenderTestHelper prerender_helper_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(crbug.com/1329881): Re-enable this test
#if BUILDFLAG(IS_MAC)
#define MAYBE_Activate_SpeculationRule DISABLED_Activate_SpeculationRule
#else
#define MAYBE_Activate_SpeculationRule Activate_SpeculationRule
#endif
IN_PROC_BROWSER_TEST_F(PrerenderPageLoadMetricsObserverBrowserTest,
                       MAYBE_Activate_SpeculationRule) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to an initial page.
  auto initial_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  // Start a prerender.
  GURL prerender_url = embedded_test_server()->GetURL("/title2.html");
  prerender_helper_.AddPrerender(prerender_url);

  histogram_tester().ExpectBucketCount(
      internal::kPageLoadPrerenderObserverEvent,
      internal::PageLoadPrerenderObserverEvent::kOnPrerenderStart, 1);

  // Activate and wait for FCP.
  auto waiter = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
      web_contents());
  waiter->AddPageExpectation(page_load_metrics::PageLoadMetricsTestWaiter::
                                 TimingField::kFirstContentfulPaint);
  prerender_helper_.NavigatePrimaryPage(prerender_url);
  waiter->Wait();

  histogram_tester().ExpectBucketCount(
      page_load_metrics::internal::kPageLoadPrerender2VisibilityAtActivation,
      page_load_metrics::internal::VisibilityAtActivation::kVisible, 1);
  histogram_tester().ExpectBucketCount(
      internal::kPageLoadPrerenderObserverEvent,
      internal::PageLoadPrerenderObserverEvent::kDidActivatePrerenderedPage, 1);
  histogram_tester().ExpectTotalCount(
      prerender_helper_.GenerateHistogramName(
          internal::kHistogramPrerenderNavigationToActivation,
          content::PrerenderTriggerType::kSpeculationRule, ""),
      1);

  // Expect only FP and FCP for prerender are recorded.
  CheckFirstPaintMetrics();
  CheckFirstContentfulPaintMetrics();

  // Simulate mouse click and wait for FirstInputDelay.
  content::SimulateMouseClick(web_contents(), 0,
                              blink::WebPointerProperties::Button::kLeft);
  waiter->AddPageExpectation(page_load_metrics::PageLoadMetricsTestWaiter::
                                 TimingField::kFirstInputDelay);
  waiter->Wait();

  // Expect only FID for prerender is recorded.
  CheckFirstInputDelayMetrics();

  // Force navigation to another page, which should force logging of histograms
  // persisted at the end of the page load lifetime.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  // Expect only LCP for prerender is recorded.
  CheckLargestContentfulPaintMetrics();

  histogram_tester().ExpectBucketCount(
      internal::kPageLoadPrerenderObserverEvent,
      internal::PageLoadPrerenderObserverEvent::kRecordLayoutShiftScoreMetrics,
      1);
  histogram_tester().ExpectTotalCount(
      prerender_helper_.GenerateHistogramName(
          internal::kHistogramPrerenderCumulativeShiftScore,
          content::PrerenderTriggerType::kSpeculationRule, ""),
      1);
  histogram_tester().ExpectTotalCount(
      prerender_helper_.GenerateHistogramName(
          internal::kHistogramPrerenderCumulativeShiftScoreMainFrame,
          content::PrerenderTriggerType::kSpeculationRule, ""),
      1);
  histogram_tester().ExpectTotalCount(
      prerender_helper_.GenerateHistogramName(
          internal::
              kHistogramPrerenderMaxCumulativeShiftScoreSessionWindowGap1000msMax5000ms2,
          content::PrerenderTriggerType::kSpeculationRule, ""),
      1);

  auto entries = GetMergedUkmEntries(PrerenderPageLoad::kEntryName);
  EXPECT_EQ(2u, entries.size());

  const ukm::mojom::UkmEntry* prerendered_page_entry =
      entries[prerender_url].get();
  ASSERT_TRUE(prerendered_page_entry);
  EXPECT_FALSE(ukm_recorder().EntryHasMetric(
      prerendered_page_entry, PrerenderPageLoad::kTriggeredPrerenderName));
  ukm_recorder().ExpectEntryMetric(prerendered_page_entry,
                                   PrerenderPageLoad::kWasPrerenderedName, 1);
  EXPECT_TRUE(ukm_recorder().EntryHasMetric(
      prerendered_page_entry,
      PrerenderPageLoad::kTiming_NavigationToActivationName));
  EXPECT_TRUE(ukm_recorder().EntryHasMetric(
      prerendered_page_entry,
      PrerenderPageLoad::kTiming_ActivationToFirstContentfulPaintName));
  EXPECT_TRUE(ukm_recorder().EntryHasMetric(
      prerendered_page_entry,
      PrerenderPageLoad::kTiming_ActivationToLargestContentfulPaintName));
  EXPECT_TRUE(ukm_recorder().EntryHasMetric(
      prerendered_page_entry,
      PrerenderPageLoad::kInteractiveTiming_FirstInputDelay4Name));
  EXPECT_TRUE(ukm_recorder().EntryHasMetric(
      prerendered_page_entry,
      PrerenderPageLoad::
          kLayoutInstability_MaxCumulativeShiftScore_SessionWindow_Gap1000ms_Max5000msName));
  EXPECT_FALSE(ukm_recorder().EntryHasMetric(
      prerendered_page_entry,
      PageLoad::kPaintTiming_NavigationToFirstContentfulPaintName));
  EXPECT_FALSE(ukm_recorder().EntryHasMetric(
      prerendered_page_entry,
      PageLoad::kPaintTiming_NavigationToLargestContentfulPaint2Name));
  // Expect that when the response has no Cache-control:no-store we still record
  // the `kMainFrameResource_RequestHasNoStoreName` metric for prerender.
  EXPECT_TRUE(ukm_recorder().EntryHasMetric(
      prerendered_page_entry,
      PrerenderPageLoad::kMainFrameResource_RequestHasNoStoreName));

  const ukm::mojom::UkmEntry* initiator_page_entry = entries[initial_url].get();
  ASSERT_TRUE(initiator_page_entry);
  ukm_recorder().ExpectEntryMetric(
      initiator_page_entry, PrerenderPageLoad::kTriggeredPrerenderName, 1);
  EXPECT_FALSE(ukm_recorder().EntryHasMetric(
      initiator_page_entry, PrerenderPageLoad::kWasPrerenderedName));
  EXPECT_FALSE(ukm_recorder().EntryHasMetric(
      initiator_page_entry,
      PrerenderPageLoad::kTiming_NavigationToActivationName));
  EXPECT_FALSE(ukm_recorder().EntryHasMetric(
      initiator_page_entry,
      PrerenderPageLoad::kTiming_ActivationToFirstContentfulPaintName));
  EXPECT_FALSE(ukm_recorder().EntryHasMetric(
      initiator_page_entry,
      PrerenderPageLoad::kTiming_ActivationToLargestContentfulPaintName));

  CheckResponsivenessMetrics(prerender_url);
}

// Tests that metrics are not recorded if the page moves to background before
// recording metrics is completed.
IN_PROC_BROWSER_TEST_F(PrerenderPageLoadMetricsObserverBrowserTest,
                       ActivateAndMoveToBackground_SpeculationRule) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to an initial page.
  auto initial_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  // Start a prerender.
  GURL prerender_url = embedded_test_server()->GetURL("/title2.html");
  prerender_helper_.AddPrerender(prerender_url);

  histogram_tester().ExpectBucketCount(
      internal::kPageLoadPrerenderObserverEvent,
      internal::PageLoadPrerenderObserverEvent::kOnPrerenderStart, 1);

  // Start an activation.
  prerender_helper_.NavigatePrimaryPage(prerender_url);

  // Changing the visibility state to HIDDEN will prevent from recording metrics
  // such as LCP since they are supposed to be recorded only when the page is
  // foreground.
  web_contents()->WasHidden();

  // Force navigation to another page, which should force logging of metrics
  // persisted at the end of the page load lifetime.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  // The visibility at activation should be visible as the page gets hidden
  // after prerender activation starts.
  histogram_tester().ExpectBucketCount(
      page_load_metrics::internal::kPageLoadPrerender2VisibilityAtActivation,
      page_load_metrics::internal::VisibilityAtActivation::kVisible, 1);

  histogram_tester().ExpectBucketCount(
      internal::kPageLoadPrerenderObserverEvent,
      internal::PageLoadPrerenderObserverEvent::kOnComplete, 1);
  histogram_tester().ExpectBucketCount(
      internal::kPageLoadPrerenderObserverEvent,
      internal::PageLoadPrerenderObserverEvent::kRecordSessionEndHistograms, 1);

  auto entries = GetMergedUkmEntries(PrerenderPageLoad::kEntryName);
  EXPECT_EQ(2u, entries.size());

  const ukm::mojom::UkmEntry* prerendered_page_entry =
      entries[prerender_url].get();
  ASSERT_TRUE(prerendered_page_entry);
  // `WasPrerendered` exists since it's recorded when the activation starts.
  EXPECT_TRUE(ukm_recorder().EntryHasMetric(
      prerendered_page_entry, PrerenderPageLoad::kWasPrerenderedName));

  // LCP for prerender shouldn't be recorded since the page is in the
  // background.
  EXPECT_FALSE(ukm_recorder().EntryHasMetric(
      prerendered_page_entry,
      PrerenderPageLoad::kTiming_ActivationToLargestContentfulPaintName));

  histogram_tester().ExpectTotalCount(
      prerender_helper_.GenerateHistogramName(
          internal::kHistogramPrerenderActivationToLargestContentfulPaint2,
          content::PrerenderTriggerType::kSpeculationRule, ""),
      0);
}

// TODO(crbug.com/1329881): Re-enable this test
#if BUILDFLAG(IS_MAC)
#define MAYBE_Activate_Embedder_DirectURLInput \
  DISABLED_Activate_Embedder_DirectURLInput
#else
#define MAYBE_Activate_Embedder_DirectURLInput Activate_Embedder_DirectURLInput
#endif
IN_PROC_BROWSER_TEST_F(PrerenderPageLoadMetricsObserverBrowserTest,
                       MAYBE_Activate_Embedder_DirectURLInput) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL prerender_url = embedded_test_server()->GetURL("/title2.html");

  // Hold PrerenderHandle while the test is executed to avoid canceling a
  // prerender host in the destruction of PrerenderHandle.
  std::unique_ptr<content::PrerenderHandle> prerender_handle =
      prerender_helper_.AddEmbedderTriggeredPrerenderAsync(
          prerender_url, content::PrerenderTriggerType::kEmbedder,
          prerender_utils::kDirectUrlInputMetricSuffix,
          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR));
  EXPECT_TRUE(prerender_handle);

  // Wait until the completion of prerendering navigation.
  prerender_helper_.WaitForPrerenderLoadCompletion(prerender_url);
  int host_id = prerender_helper_.GetHostForUrl(prerender_url);
  EXPECT_NE(host_id, content::RenderFrameHost::kNoFrameTreeNodeId);

  histogram_tester().ExpectBucketCount(
      internal::kPageLoadPrerenderObserverEvent,
      internal::PageLoadPrerenderObserverEvent::kOnPrerenderStart, 1);

  // Activate and wait for FCP.
  auto waiter = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
      web_contents());
  waiter->AddPageExpectation(page_load_metrics::PageLoadMetricsTestWaiter::
                                 TimingField::kFirstContentfulPaint);
  // Simulate a browser-initiated navigation.
  web_contents()->OpenURL(content::OpenURLParams(
      prerender_url, content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
      /*is_renderer_initiated=*/false));
  waiter->Wait();

  histogram_tester().ExpectBucketCount(
      page_load_metrics::internal::kPageLoadPrerender2VisibilityAtActivation,
      page_load_metrics::internal::VisibilityAtActivation::kVisible, 1);
  histogram_tester().ExpectBucketCount(
      internal::kPageLoadPrerenderObserverEvent,
      internal::PageLoadPrerenderObserverEvent::kDidActivatePrerenderedPage, 1);
  histogram_tester().ExpectTotalCount(
      prerender_helper_.GenerateHistogramName(
          internal::kHistogramPrerenderNavigationToActivation,
          content::PrerenderTriggerType::kEmbedder,
          prerender_utils::kDirectUrlInputMetricSuffix),
      1);

  // Expect only FP and FCP for prerender are recorded.
  CheckFirstPaintMetrics(content::PrerenderTriggerType::kEmbedder,
                         prerender_utils::kDirectUrlInputMetricSuffix);
  CheckFirstContentfulPaintMetrics(
      content::PrerenderTriggerType::kEmbedder,
      prerender_utils::kDirectUrlInputMetricSuffix);

  // Simulate mouse click and wait for FirstInputDelay.
  content::SimulateMouseClick(web_contents(), 0,
                              blink::WebPointerProperties::Button::kLeft);
  waiter->AddPageExpectation(page_load_metrics::PageLoadMetricsTestWaiter::
                                 TimingField::kFirstInputDelay);
  waiter->Wait();

  // Expect only FID for prerender is recorded.
  CheckFirstInputDelayMetrics(content::PrerenderTriggerType::kEmbedder,
                              prerender_utils::kDirectUrlInputMetricSuffix);

  // Force navigation to another page, which should force logging of
  // histograms persisted at the end of the page load lifetime.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  // Expect only LCP for prerender is recorded.
  CheckLargestContentfulPaintMetrics(
      content::PrerenderTriggerType::kEmbedder,
      prerender_utils::kDirectUrlInputMetricSuffix);

  histogram_tester().ExpectBucketCount(
      internal::kPageLoadPrerenderObserverEvent,
      internal::PageLoadPrerenderObserverEvent::kRecordLayoutShiftScoreMetrics,
      1);
  histogram_tester().ExpectTotalCount(
      prerender_helper_.GenerateHistogramName(
          internal::kHistogramPrerenderCumulativeShiftScore,
          content::PrerenderTriggerType::kEmbedder,
          prerender_utils::kDirectUrlInputMetricSuffix),
      1);
  histogram_tester().ExpectTotalCount(
      prerender_helper_.GenerateHistogramName(
          internal::kHistogramPrerenderCumulativeShiftScoreMainFrame,
          content::PrerenderTriggerType::kEmbedder,
          prerender_utils::kDirectUrlInputMetricSuffix),
      1);
  histogram_tester().ExpectTotalCount(
      prerender_helper_.GenerateHistogramName(
          internal::
              kHistogramPrerenderMaxCumulativeShiftScoreSessionWindowGap1000msMax5000ms2,
          content::PrerenderTriggerType::kEmbedder,
          prerender_utils::kDirectUrlInputMetricSuffix),
      1);

  CheckResponsivenessMetrics(prerender_url);
}

IN_PROC_BROWSER_TEST_F(PrerenderPageLoadMetricsObserverBrowserTest,
                       CancelledPrerender) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to an initial page.
  auto initial_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  // Start a prerender.
  GURL prerender_url = embedded_test_server()->GetURL("/title2.html");
  const int host_id = prerender_helper_.AddPrerender(prerender_url);

  histogram_tester().ExpectBucketCount(
      internal::kPageLoadPrerenderObserverEvent,
      internal::PageLoadPrerenderObserverEvent::kOnPrerenderStart, 1);

  content::test::PrerenderHostObserver observer(*web_contents(), host_id);
  prerender_helper_.CancelPrerenderedPage(host_id);
  observer.WaitForDestroyed();

  // Force navigation to another page, which should force logging of histograms
  // persisted at the end of the page load lifetime.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  histogram_tester().ExpectTotalCount(
      page_load_metrics::internal::kPageLoadPrerender2VisibilityAtActivation,
      0);
  histogram_tester().ExpectBucketCount(
      internal::kPageLoadPrerenderObserverEvent,
      internal::PageLoadPrerenderObserverEvent::kDidActivatePrerenderedPage, 0);
  histogram_tester().ExpectBucketCount(
      internal::kPageLoadPrerenderObserverEvent,
      internal::PageLoadPrerenderObserverEvent::kOnFirstPaintInPage, 0);
  histogram_tester().ExpectBucketCount(
      internal::kPageLoadPrerenderObserverEvent,
      internal::PageLoadPrerenderObserverEvent::kOnFirstContentfulPaintInPage,
      0);
  histogram_tester().ExpectBucketCount(
      internal::kPageLoadPrerenderObserverEvent,
      internal::PageLoadPrerenderObserverEvent::kOnComplete, 1);
  histogram_tester().ExpectBucketCount(
      internal::kPageLoadPrerenderObserverEvent,
      internal::PageLoadPrerenderObserverEvent::kRecordSessionEndHistograms, 1);

  // As the prerender was cancelled, no prerendering metrics are recorded.
  EXPECT_EQ(0u, histogram_tester()
                    .GetTotalCountsForPrefix("PageLoad.Clients.Prerender.")
                    .size());

  auto entries = GetMergedUkmEntries(PrerenderPageLoad::kEntryName);
  EXPECT_FALSE(base::Contains(entries, prerender_url));
}

// TODO(crbug.com/1329881): Re-enable this test
#if BUILDFLAG(IS_MAC)
#define MAYBE_Redirection DISABLED_Redirection
#else
#define MAYBE_Redirection Redirection
#endif
IN_PROC_BROWSER_TEST_F(PrerenderPageLoadMetricsObserverBrowserTest,
                       MAYBE_Redirection) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to an initial page.
  auto initial_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  // Start a prerender.
  GURL redirected_url = embedded_test_server()->GetURL("/title2.html");
  GURL prerender_url = embedded_test_server()->GetURL("/server-redirect?" +
                                                      redirected_url.spec());
  prerender_helper_.AddPrerender(prerender_url);

  histogram_tester().ExpectBucketCount(
      internal::kPageLoadPrerenderObserverEvent,
      internal::PageLoadPrerenderObserverEvent::kOnPrerenderStart, 1);

  // Activate and wait for FCP.
  auto waiter = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
      web_contents());
  waiter->AddPageExpectation(page_load_metrics::PageLoadMetricsTestWaiter::
                                 TimingField::kFirstContentfulPaint);
  prerender_helper_.NavigatePrimaryPage(prerender_url);
  waiter->Wait();

  histogram_tester().ExpectBucketCount(
      page_load_metrics::internal::kPageLoadPrerender2VisibilityAtActivation,
      page_load_metrics::internal::VisibilityAtActivation::kVisible, 1);
  histogram_tester().ExpectBucketCount(
      internal::kPageLoadPrerenderObserverEvent,
      internal::PageLoadPrerenderObserverEvent::kDidActivatePrerenderedPage, 1);
  CheckFirstPaintMetrics();
  CheckFirstContentfulPaintMetrics();

  // Force navigation to another page, which should force logging of histograms
  // persisted at the end of the page load lifetime.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  CheckLargestContentfulPaintMetrics();

  // Verify that UKM records the URL after the redirection, not the initial URL.
  auto entries = GetMergedUkmEntries(PrerenderPageLoad::kEntryName);
  ASSERT_FALSE(base::Contains(entries, prerender_url));
  const ukm::mojom::UkmEntry* prerendered_page_entry =
      entries[redirected_url].get();
  ASSERT_TRUE(prerendered_page_entry);

  EXPECT_FALSE(ukm_recorder().EntryHasMetric(
      prerendered_page_entry, PrerenderPageLoad::kTriggeredPrerenderName));
  ukm_recorder().ExpectEntryMetric(prerendered_page_entry,
                                   PrerenderPageLoad::kWasPrerenderedName, 1);
  EXPECT_TRUE(ukm_recorder().EntryHasMetric(
      prerendered_page_entry,
      PrerenderPageLoad::kTiming_NavigationToActivationName));
  EXPECT_TRUE(ukm_recorder().EntryHasMetric(
      prerendered_page_entry,
      PrerenderPageLoad::kTiming_ActivationToFirstContentfulPaintName));
  EXPECT_TRUE(ukm_recorder().EntryHasMetric(
      prerendered_page_entry,
      PrerenderPageLoad::kTiming_ActivationToLargestContentfulPaintName));
}

// Tests that metrics are recoreded correctly with Cache-control:no store when
// prerender is activated.
IN_PROC_BROWSER_TEST_F(PrerenderPageLoadMetricsObserverBrowserTest,
                       ActivationWithNoStoreResponse) {
  // Create a HTTP response to control prerendering main-frame navigation.
  net::test_server::ControllableHttpResponse main_document_response(
      embedded_test_server(), "/main_document");

  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  const GURL kPrerenderingUrl =
      embedded_test_server()->GetURL("/main_document");

  // Navigate to an initial page in primary frame tree.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kInitialUrl));

  // Start a prerender, and navigate to a page that doesn't commit navigation.
  {
    content::test::PrerenderHostRegistryObserver registry_observer(
        *web_contents());
    prerender_helper_.AddPrerenderAsync(kPrerenderingUrl);
    registry_observer.WaitForTrigger(kPrerenderingUrl);
  }

  histogram_tester().ExpectBucketCount(
      internal::kPageLoadPrerenderObserverEvent,
      internal::PageLoadPrerenderObserverEvent::kOnPrerenderStart, 1);

  int host_id = prerender_helper_.GetHostForUrl(kPrerenderingUrl);
  content::test::PrerenderHostObserver prerender_observer(*web_contents(),
                                                          host_id);
  EXPECT_FALSE(prerender_observer.was_activated());

  // Complete the prerender response with no cache and finish ongoing prerender
  // main frame navigation. Start navigation in primary page to
  // kPrerenderingUrl.
  content::TestActivationManager primary_page_manager(web_contents(),
                                                      kPrerenderingUrl);
  ASSERT_TRUE(ExecJs(web_contents()->GetPrimaryMainFrame(),
                     content::JsReplace("location = $1", kPrerenderingUrl)));

  main_document_response.WaitForRequest();
  main_document_response.Send(kResponseWithNoStore);
  main_document_response.Done();

  auto waiter = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
      web_contents());
  waiter->AddPageExpectation(page_load_metrics::PageLoadMetricsTestWaiter::
                                 TimingField::kFirstContentfulPaint);
  primary_page_manager.WaitForNavigationFinished();
  prerender_observer.WaitForActivation();
  waiter->Wait();

  histogram_tester().ExpectBucketCount(
      page_load_metrics::internal::kPageLoadPrerender2VisibilityAtActivation,
      page_load_metrics::internal::VisibilityAtActivation::kVisible, 1);
  histogram_tester().ExpectBucketCount(
      internal::kPageLoadPrerenderObserverEvent,
      internal::PageLoadPrerenderObserverEvent::kDidActivatePrerenderedPage, 1);

  // Force navigation to another page, which should force logging of metrics
  // persisted at the end of the page load lifetime.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  CheckFirstPaintMetrics();
  CheckFirstContentfulPaintMetrics();
  CheckLargestContentfulPaintMetrics();

  auto entries = GetMergedUkmEntries(PrerenderPageLoad::kEntryName);
  EXPECT_EQ(2u, entries.size());

  const ukm::mojom::UkmEntry* prerendered_page_entry =
      entries[kPrerenderingUrl].get();
  ASSERT_TRUE(prerendered_page_entry);

  // RequestHasNoStore should be recorded with value 1 as the response has
  // Cache-control no-store in it.
  EXPECT_TRUE(ukm_recorder().EntryHasMetric(
      prerendered_page_entry,
      PrerenderPageLoad::kMainFrameResource_RequestHasNoStoreName));
  ukm_recorder().ExpectEntryMetric(
      prerendered_page_entry,
      PrerenderPageLoad::kMainFrameResource_RequestHasNoStoreName, 1);

  EXPECT_TRUE(ukm_recorder().EntryHasMetric(
      prerendered_page_entry,
      PrerenderPageLoad::kTiming_ActivationToLargestContentfulPaintName));
}

// Tests that metrics are recoreded correctly for loading main resource of an
// activated page.
IN_PROC_BROWSER_TEST_F(PrerenderPageLoadMetricsObserverBrowserTest,
                       MainResourceLoadEvent) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to an initial page.
  auto initial_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), initial_url));

  // Start a prerender.
  GURL prerender_url = embedded_test_server()->GetURL("/title2.html");
  GURL prerender_iframe_url = embedded_test_server()->GetURL("/title1.html");
  prerender_helper_.AddPrerender(prerender_url);
  prerender_helper_.WaitForPrerenderLoadCompletion(prerender_url);
  content::test::PrerenderHostObserver observer(*web_contents(), prerender_url);
  prerender_helper_.NavigatePrimaryPage(prerender_url);

  // Activate the page and add a new iframe.
  observer.WaitForActivation();
  std::string add_iframe_script = R"(
    function add_iframe(url) {
      const frame = document.createElement('iframe');
      frame.src = url;
      document.body.appendChild(frame);
      return new Promise(resolve => {
        frame.onload = e => resolve('LOADED');
      });
    }
    add_iframe($1);
  )";
  EXPECT_EQ("LOADED",
            EvalJs(web_contents(), content::JsReplace(add_iframe_script,
                                                      prerender_iframe_url)));

  // Flush metrics.
  ASSERT_TRUE(
      content::NavigateToURL(web_contents(), GURL(url::kAboutBlankURL)));

  // Record the result for only once.
  histogram_tester().ExpectUniqueSample(
      prerender_helper_.GenerateHistogramName(
          "PageLoad.Internal.Prerender2.ActivatedPageLoaderStatus",
          content::PrerenderTriggerType::kSpeculationRule, ""),
      net::Error::OK, 1);
}

IN_PROC_BROWSER_TEST_F(PrerenderPageLoadMetricsObserverBrowserTest,
                       MainFrameNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to an initial page.
  auto initial_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  // Start a prerender and a main frame navigation in the prerendered page.
  GURL prerender_url = embedded_test_server()->GetURL("/title2.html");
  GURL navigation_url = embedded_test_server()->GetURL("/title3.html");
  int host_id = prerender_helper_.AddPrerender(prerender_url);
  prerender_helper_.WaitForPrerenderLoadCompletion(host_id);
  prerender_helper_.NavigatePrerenderedPage(host_id, navigation_url);
  prerender_helper_.WaitForPrerenderLoadCompletion(host_id);

  // Expect that OnPrerenderStart is called twice for the initial prerender
  // navigation and the main frame navigation in the prerendered page.
  histogram_tester().ExpectBucketCount(
      internal::kPageLoadPrerenderObserverEvent,
      internal::PageLoadPrerenderObserverEvent::kOnPrerenderStart, 2);

  // Activate and wait for FCP.
  auto waiter = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
      web_contents());
  waiter->AddPageExpectation(page_load_metrics::PageLoadMetricsTestWaiter::
                                 TimingField::kFirstContentfulPaint);
  prerender_helper_.NavigatePrimaryPage(prerender_url);
  waiter->Wait();

  // "PageLoad.Internal.Prerender2.VisibilityAtActivation" is recorded only once
  // when the activation is finished. It's not recorded for the initial
  // prerendering navigation.
  histogram_tester().ExpectBucketCount(
      page_load_metrics::internal::kPageLoadPrerender2VisibilityAtActivation,
      page_load_metrics::internal::VisibilityAtActivation::kVisible, 1);
  histogram_tester().ExpectBucketCount(
      page_load_metrics::internal::kPageLoadPrerender2VisibilityAtActivation,
      page_load_metrics::internal::VisibilityAtActivation::kOccluded, 0);
  histogram_tester().ExpectBucketCount(
      page_load_metrics::internal::kPageLoadPrerender2VisibilityAtActivation,
      page_load_metrics::internal::VisibilityAtActivation::kHidden, 0);

  histogram_tester().ExpectBucketCount(
      internal::kPageLoadPrerenderObserverEvent,
      internal::PageLoadPrerenderObserverEvent::kDidActivatePrerenderedPage, 1);
  histogram_tester().ExpectTotalCount(
      prerender_helper_.GenerateHistogramName(
          internal::kHistogramPrerenderNavigationToActivation,
          content::PrerenderTriggerType::kSpeculationRule, ""),
      1);

  // Expect only FP and FCP for prerender are recorded.
  CheckFirstPaintMetrics();
  CheckFirstContentfulPaintMetrics();

  // Simulate mouse click and wait for FirstInputDelay.
  content::SimulateMouseClick(web_contents(), 0,
                              blink::WebPointerProperties::Button::kLeft);
  waiter->AddPageExpectation(page_load_metrics::PageLoadMetricsTestWaiter::
                                 TimingField::kFirstInputDelay);
  waiter->Wait();

  // Expect only FID for prerender is recorded.
  CheckFirstInputDelayMetrics();

  // Force navigation to another page, which should force logging of histograms
  // persisted at the end of the page load lifetime.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  // Expect only LCP for prerender is recorded.
  // PrerenderPageLoadMetricsObserver::RecordSessionEndHistograms and OnComplete
  // are called twice but LCP is recorded only once because the initial
  // prerendered page is finished in the background.
  histogram_tester().ExpectBucketCount(
      internal::kPageLoadPrerenderObserverEvent,
      internal::PageLoadPrerenderObserverEvent::kOnComplete, 2);
  histogram_tester().ExpectBucketCount(
      internal::kPageLoadPrerenderObserverEvent,
      internal::PageLoadPrerenderObserverEvent::kRecordSessionEndHistograms, 2);
  histogram_tester().ExpectBucketCount(
      "PageLoad.Internal.Prerender2.ForegroundCheckResult."
      "LargestContentfulPaint",
      internal::PageLoadPrerenderForegroundCheckResult::kPassed, 1);

  // LargestContentfulPaint should be recorded in the prerender PageLoad, not
  // in the regular PageLoad.
  histogram_tester().ExpectTotalCount(
      prerender_helper_.GenerateHistogramName(
          internal::kHistogramPrerenderActivationToLargestContentfulPaint2,
          content::PrerenderTriggerType::kSpeculationRule,
          /*embedder_suffix=*/""),
      1);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramLargestContentfulPaint, 0);

  // Expect CLS for prerender is recorded.
  histogram_tester().ExpectBucketCount(
      internal::kPageLoadPrerenderObserverEvent,
      internal::PageLoadPrerenderObserverEvent::kRecordLayoutShiftScoreMetrics,
      1);
  histogram_tester().ExpectTotalCount(
      prerender_helper_.GenerateHistogramName(
          internal::kHistogramPrerenderCumulativeShiftScore,
          content::PrerenderTriggerType::kSpeculationRule, ""),
      1);
  histogram_tester().ExpectTotalCount(
      prerender_helper_.GenerateHistogramName(
          internal::kHistogramPrerenderCumulativeShiftScoreMainFrame,
          content::PrerenderTriggerType::kSpeculationRule, ""),
      1);
  histogram_tester().ExpectTotalCount(
      prerender_helper_.GenerateHistogramName(
          internal::
              kHistogramPrerenderMaxCumulativeShiftScoreSessionWindowGap1000msMax5000ms2,
          content::PrerenderTriggerType::kSpeculationRule, ""),
      1);

  // Verify that UKM records the URL (`navigation_url`) after the navigation,
  // not the initial URL.
  auto entries = GetMergedUkmEntries(PrerenderPageLoad::kEntryName);
  EXPECT_EQ(2u, entries.size());
  ASSERT_FALSE(base::Contains(entries, prerender_url));
  const ukm::mojom::UkmEntry* prerendered_page_entry =
      entries[navigation_url].get();
  ASSERT_TRUE(prerendered_page_entry);

  EXPECT_FALSE(ukm_recorder().EntryHasMetric(
      prerendered_page_entry, PrerenderPageLoad::kTriggeredPrerenderName));
  ukm_recorder().ExpectEntryMetric(prerendered_page_entry,
                                   PrerenderPageLoad::kWasPrerenderedName, 1);
  EXPECT_TRUE(ukm_recorder().EntryHasMetric(
      prerendered_page_entry,
      PrerenderPageLoad::kTiming_NavigationToActivationName));
  EXPECT_TRUE(ukm_recorder().EntryHasMetric(
      prerendered_page_entry,
      PrerenderPageLoad::kTiming_ActivationToFirstContentfulPaintName));
  EXPECT_TRUE(ukm_recorder().EntryHasMetric(
      prerendered_page_entry,
      PrerenderPageLoad::kTiming_ActivationToLargestContentfulPaintName));
  EXPECT_TRUE(ukm_recorder().EntryHasMetric(
      prerendered_page_entry,
      PrerenderPageLoad::kInteractiveTiming_FirstInputDelay4Name));
  EXPECT_TRUE(ukm_recorder().EntryHasMetric(
      prerendered_page_entry,
      PrerenderPageLoad::
          kLayoutInstability_MaxCumulativeShiftScore_SessionWindow_Gap1000ms_Max5000msName));
  EXPECT_FALSE(ukm_recorder().EntryHasMetric(
      prerendered_page_entry,
      PageLoad::kPaintTiming_NavigationToFirstContentfulPaintName));
  EXPECT_FALSE(ukm_recorder().EntryHasMetric(
      prerendered_page_entry,
      PageLoad::kPaintTiming_NavigationToLargestContentfulPaint2Name));

  const ukm::mojom::UkmEntry* initiator_page_entry = entries[initial_url].get();
  ASSERT_TRUE(initiator_page_entry);
  ukm_recorder().ExpectEntryMetric(
      initiator_page_entry, PrerenderPageLoad::kTriggeredPrerenderName, 1);
  EXPECT_FALSE(ukm_recorder().EntryHasMetric(
      initiator_page_entry, PrerenderPageLoad::kWasPrerenderedName));
  EXPECT_FALSE(ukm_recorder().EntryHasMetric(
      initiator_page_entry,
      PrerenderPageLoad::kTiming_NavigationToActivationName));
  EXPECT_FALSE(ukm_recorder().EntryHasMetric(
      initiator_page_entry,
      PrerenderPageLoad::kTiming_ActivationToFirstContentfulPaintName));
  EXPECT_FALSE(ukm_recorder().EntryHasMetric(
      initiator_page_entry,
      PrerenderPageLoad::kTiming_ActivationToLargestContentfulPaintName));

  CheckResponsivenessMetrics(navigation_url);
}
