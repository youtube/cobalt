// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/renderer_host/back_forward_cache_impl.h"
#include "content/browser/renderer_host/back_forward_cache_metrics.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "services/device/public/cpp/test/scoped_geolocation_overrider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/scheduler/web_scheduler_tracked_feature.h"

using base::Bucket;
using testing::ElementsAre;

namespace content {

namespace {

ukm::SourceId ToSourceId(int64_t navigation_id) {
  return ukm::ConvertToSourceId(navigation_id,
                                ukm::SourceIdType::NAVIGATION_ID);
}

// Some features are present in almost all page loads (especially the ones
// which are related to the document finishing loading).
// We ignore them to make tests easier to read and write.

constexpr blink::scheduler::WebSchedulerTrackedFeatures kFeaturesToIgnore =
    blink::scheduler::WebSchedulerTrackedFeatures(
        blink::scheduler::WebSchedulerTrackedFeature::kDocumentLoaded,
        blink::scheduler::WebSchedulerTrackedFeature::
            kOutstandingNetworkRequestFetch,
        blink::scheduler::WebSchedulerTrackedFeature::
            kOutstandingNetworkRequestXHR,
        blink::scheduler::WebSchedulerTrackedFeature::
            kOutstandingNetworkRequestOthers);

using UkmMetrics = ukm::TestUkmRecorder::HumanReadableUkmMetrics;
using UkmEntry = ukm::TestUkmRecorder::HumanReadableUkmEntry;

enum BackForwardCacheStatus { kDisabled = 0, kEnabled = 1 };
}  // namespace

class BackForwardCacheMetricsBrowserTestBase : public ContentBrowserTest,
                                               public WebContentsObserver {
 public:
  BackForwardCacheMetricsBrowserTestBase() {
    geolocation_override_ =
        std::make_unique<device::ScopedGeolocationOverrider>(1.0, 1.0);
  }

 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
    WebContentsObserver::Observe(shell()->web_contents());
  }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  void GiveItSomeTime() {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(200));
    run_loop.Run();
  }

  RenderFrameHostImpl* current_frame_host() {
    return web_contents()->GetPrimaryFrameTree().root()->current_frame_host();
  }

  void DidStartNavigation(NavigationHandle* navigation_handle) override {
    navigation_ids_.push_back(navigation_handle->GetNavigationId());
  }

  void NavigateAndWaitForDisablingFeature(
      const GURL& url,
      blink::scheduler::WebSchedulerTrackedFeature feature) {
    base::RunLoop run_loop;
    current_frame_host()
        ->SetBackForwardCacheDisablingFeaturesCallbackForTesting(
            base::BindLambdaForTesting(
                [&run_loop, feature](
                    blink::scheduler::WebSchedulerTrackedFeatures features) {
                  if (features.Has(feature) && run_loop.running())
                    run_loop.Quit();
                }));
    EXPECT_TRUE(NavigateToURL(shell(), url));
    run_loop.Run();

    EXPECT_EQ(base::Difference(
                  current_frame_host()->GetBackForwardCacheDisablingFeatures(),
                  kFeaturesToIgnore),
              blink::scheduler::WebSchedulerTrackedFeatures(feature));

    // Close the web contents to ensure that no new notifications arrive to the
    // function local callback above after this function has returned.
    web_contents()->Close();
  }

  std::vector<int64_t> navigation_ids_;

 private:
  std::unique_ptr<device::ScopedGeolocationOverrider> geolocation_override_;
};

class BackForwardCacheMetricsBrowserTest
    : public BackForwardCacheMetricsBrowserTestBase,
      public testing::WithParamInterface<BackForwardCacheStatus> {
 public:
  BackForwardCacheMetricsBrowserTest() {
    if (GetParam() == BackForwardCacheStatus::kEnabled) {
      // Enable BackForwardCache.
      feature_list_.InitWithFeaturesAndParameters(
          {{features::kBackForwardCache, {}},
           {kBackForwardCacheNoTimeEviction, {}}},
          // Allow BackForwardCache for all devices regardless of their memory.
          {features::kBackForwardCacheMemoryControls});
      DCHECK(IsBackForwardCacheEnabled());
    } else {
      feature_list_.InitAndDisableFeature(features::kBackForwardCache);
      DCHECK(!IsBackForwardCacheEnabled());
    }
  }

  static std::string DescribeParams(
      const testing::TestParamInfo<ParamType>& info) {
    return info.param ? "BFCacheEnabled" : "BFCacheDisabled";
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(BackForwardCacheMetricsBrowserTest, UKM) {
  ukm::TestAutoSetUkmRecorder recorder;

  const GURL url1(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_one_frame.html"));
  const GURL url2(embedded_test_server()->GetURL("b.com", "/title1.html"));
  const char kChildFrameId[] = "child0";

  EXPECT_TRUE(NavigateToURL(shell(), url2));
  // The navigation entries are:
  // [*url2].

  EXPECT_TRUE(NavigateToURL(shell(), url1));
  // The navigation entries are:
  // [url2, *url1(subframe)].

  EXPECT_TRUE(
      NavigateIframeToURL(shell()->web_contents(), kChildFrameId, url2));
  // The navigation entries are:
  // [url2, url1(subframe), *url1(url2)].

  EXPECT_TRUE(NavigateToURL(shell(), url2));
  // The navigation entries are:
  // [url2, url1(subframe), url1(url2), *url2].

  {
    // We are waiting for two navigations here: main frame and subframe.
    // However, when back/forward cache is enabled, back navigation to a page
    // with subframes will not trigger a subframe navigation (since the
    // subframe is cached with the page).
    TestNavigationObserver navigation_observer(
        shell()->web_contents(), 1 + (IsBackForwardCacheEnabled() ? 0 : 1));
    shell()->GoBackOrForward(-1);
    navigation_observer.WaitForNavigationFinished();
  }
  // The navigation entries are:
  // [url2, url1(subframe), *url1(url2), url2].

  {
    TestNavigationObserver navigation_observer(shell()->web_contents());
    shell()->GoBackOrForward(-1);
    navigation_observer.WaitForNavigationFinished();
  }
  // The navigation entries are:
  // [url2, *url1(subframe), url1(url2), url2].

  {
    TestNavigationObserver navigation_observer(shell()->web_contents());
    shell()->GoBackOrForward(-1);
    navigation_observer.WaitForNavigationFinished();
  }
  // The navigation entries are:
  // [*url2, url1(subframe), url1(url2), url2].

  // There are five new navigations and four back navigations (3 if
  // back/forward cache is enabled, as the first subframe back navigation won't
  // happen).
  // Navigations 1, 2, 5 are main frame. Navigation 3 is an initial navigation
  // in the subframe, navigation 4 is a subframe navigation.
  ASSERT_EQ(navigation_ids_.size(),
            static_cast<size_t>(8 + (IsBackForwardCacheEnabled() ? 0 : 1)));
  ukm::SourceId id1 = ToSourceId(navigation_ids_[0]);
  ukm::SourceId id2 = ToSourceId(navigation_ids_[1]);
  // ukm::SourceId id3 = ToSourceId(navigation_ids_[2]);
  // ukm::SourceId id4 = ToSourceId(navigation_ids_[3]);
  // ukm::SourceId id5 = ToSourceId(navigation_ids_[4]);
  ukm::SourceId id6 = ToSourceId(navigation_ids_[5]);
  // ukm::SourceId id7 = ToSourceId(navigation_ids_[6]);
  // ukm::SourceId id8 = ToSourceId(navigation_ids_[7]);
  ukm::SourceId id_last =
      ToSourceId(navigation_ids_[navigation_ids_.size() - 1]);

  std::string last_navigation_id =
      "LastCommittedCrossDocumentNavigationSourceIdForTheSameDocument";

  // First back/forward navigation (#6) navigates back to navigation #3, but it
  // is the subframe navigation, so the corresponding main frame navigation is
  // #2. Navigation #7 loads the subframe for navigation #6.
  // Second back/forward navigation (#8) navigates back to navigation #2,
  // but it is subframe navigation and not reflected here. Third back/forward
  // navigation (#9) navigates back to navigation #1.
  EXPECT_THAT(
      recorder.GetEntries("HistoryNavigation", {last_navigation_id}),
      testing::ElementsAre(UkmEntry{id6, {{last_navigation_id, id2}}},
                           UkmEntry{id_last, {{last_navigation_id, id1}}}));
}

IN_PROC_BROWSER_TEST_P(BackForwardCacheMetricsBrowserTest, CloneAndGoBack) {
  ukm::TestAutoSetUkmRecorder recorder;

  const GURL url1(embedded_test_server()->GetURL("/title1.html"));
  const GURL url2(embedded_test_server()->GetURL("/title2.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url1));
  EXPECT_TRUE(NavigateToURL(shell(), url2));

  // Clone the tab and load the page.
  std::unique_ptr<WebContents> new_tab = shell()->web_contents()->Clone();
  WebContentsImpl* new_tab_impl = static_cast<WebContentsImpl*>(new_tab.get());
  WebContentsObserver::Observe(new_tab.get());
  NavigationController& new_controller = new_tab_impl->GetController();
  {
    TestNavigationObserver clone_observer(new_tab.get());
    new_controller.LoadIfNecessary();
    clone_observer.Wait();
  }

  {
    TestNavigationObserver navigation_observer(new_tab.get());
    new_controller.GoBack();
    navigation_observer.WaitForNavigationFinished();
  }

  {
    TestNavigationObserver navigation_observer(new_tab.get());
    new_controller.GoForward();
    navigation_observer.WaitForNavigationFinished();
  }

  {
    TestNavigationObserver navigation_observer(new_tab.get());
    new_controller.GoBack();
    navigation_observer.WaitForNavigationFinished();
  }

  ASSERT_EQ(navigation_ids_.size(), static_cast<size_t>(6));
  // ukm::SourceId id1 = ToSourceId(navigation_ids_[0]);
  // ukm::SourceId id2 = ToSourceId(navigation_ids_[1]);
  ukm::SourceId id3 = ToSourceId(navigation_ids_[2]);
  ukm::SourceId id4 = ToSourceId(navigation_ids_[3]);
  ukm::SourceId id5 = ToSourceId(navigation_ids_[4]);
  ukm::SourceId id6 = ToSourceId(navigation_ids_[5]);

  std::string last_navigation_id =
      "LastCommittedCrossDocumentNavigationSourceIdForTheSameDocument";

  // First two new navigations happen in the original tab.
  // The third navigation reloads the tab for the cloned WebContents.
  // The fourth goes back, and records an empty entry because its
  // NavigationEntry was cloned and the metrics objects was empty.
  // The last two navigations records non-empty entries.
  EXPECT_THAT(recorder.GetEntries("HistoryNavigation", {last_navigation_id}),
              testing::ElementsAre(UkmEntry{id4, {}},
                                   UkmEntry{id5, {{last_navigation_id, id3}}},
                                   UkmEntry{id6, {{last_navigation_id, id4}}}));

  // Ensure that for the first navigation, we record "session restored" as one
  // of the NotRestoredReasons, which is added by the metrics recording code.
  std::string not_restored_reasons = "BackForwardCache.NotRestoredReasons";
  std::vector<ukm::TestAutoSetUkmRecorder::HumanReadableUkmMetrics>
      recorded_not_restored_reasons =
          recorder.FilteredHumanReadableMetricForEntry("HistoryNavigation",
                                                       not_restored_reasons);
  ASSERT_EQ(recorded_not_restored_reasons.size(), 3u);
  EXPECT_EQ(recorded_not_restored_reasons[0][not_restored_reasons],
            1 << static_cast<int>(
                BackForwardCacheMetrics::NotRestoredReason::kSessionRestored));
}

// Confirms that UKMs are not recorded on reloading.
IN_PROC_BROWSER_TEST_P(BackForwardCacheMetricsBrowserTest, Reload) {
  ukm::TestAutoSetUkmRecorder recorder;

  const GURL url1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  const GURL url2(embedded_test_server()->GetURL("b.com", "/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url1));
  EXPECT_TRUE(NavigateToURL(shell(), url2));

  ASSERT_TRUE(HistoryGoBack(web_contents()));

  web_contents()->GetController().Reload(ReloadType::NORMAL, false);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  EXPECT_TRUE(NavigateToURL(shell(), url2));
  ASSERT_TRUE(HistoryGoBack(web_contents()));

  ASSERT_EQ(navigation_ids_.size(), static_cast<size_t>(6));
  ukm::SourceId id1 = ToSourceId(navigation_ids_[0]);
  // ukm::SourceId id2 = ToSourceId(navigation_ids_[1]);
  ukm::SourceId id3 = ToSourceId(navigation_ids_[2]);
  ukm::SourceId id4 = ToSourceId(navigation_ids_[3]);
  // ukm::SourceId id5 = ToSourceId(navigation_ids_[4]);
  ukm::SourceId id6 = ToSourceId(navigation_ids_[5]);

  // The last navigation is for reloading, and the UKM is not recorded for this
  // navigation. This also checks relaoding makes a different navigation ID.
  std::string last_navigation_id =
      "LastCommittedCrossDocumentNavigationSourceIdForTheSameDocument";
  EXPECT_THAT(recorder.GetEntries("HistoryNavigation", {last_navigation_id}),
              testing::ElementsAre(UkmEntry{id3, {{last_navigation_id, id1}}},
                                   UkmEntry{id6, {{last_navigation_id, id4}}}));
}

IN_PROC_BROWSER_TEST_P(BackForwardCacheMetricsBrowserTest,
                       SameDocumentNavigationAndGoBackImmediately) {
  ukm::TestAutoSetUkmRecorder recorder;

  const GURL url1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  const GURL url1_foo(
      embedded_test_server()->GetURL("a.com", "/title1.html#foo"));

  EXPECT_TRUE(NavigateToURL(shell(), url1));
  EXPECT_TRUE(NavigateToURL(shell(), url1_foo));

  ASSERT_TRUE(HistoryGoBack(web_contents()));

  ASSERT_EQ(navigation_ids_.size(), static_cast<size_t>(3));
  std::string last_navigation_id =
      "LastCommittedCrossDocumentNavigationSourceIdForTheSameDocument";
  // Ensure that UKMs are not recorded for the same-document history
  // navigations.
  EXPECT_THAT(recorder.GetEntries("HistoryNavigation", {last_navigation_id}),
              testing::ElementsAre());
}

IN_PROC_BROWSER_TEST_P(BackForwardCacheMetricsBrowserTest,
                       GoBackToSameDocumentNavigationEntry) {
  ukm::TestAutoSetUkmRecorder recorder;

  const GURL url1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  const GURL url1_foo(
      embedded_test_server()->GetURL("a.com", "/title1.html#foo"));
  const GURL url2(embedded_test_server()->GetURL("b.com", "/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url1));
  EXPECT_TRUE(NavigateToURL(shell(), url1_foo));
  EXPECT_TRUE(NavigateToURL(shell(), url2));

  ASSERT_TRUE(HistoryGoBack(web_contents()));

  ASSERT_EQ(navigation_ids_.size(), static_cast<size_t>(4));
  ukm::SourceId id1 = ToSourceId(navigation_ids_[0]);
  // ukm::SourceId id2 = ToSourceId(navigation_ids_[1]);
  // ukm::SourceId id3 = ToSourceId(navigation_ids_[2]);
  ukm::SourceId id4 = ToSourceId(navigation_ids_[3]);

  std::string last_navigation_id =
      "LastCommittedCrossDocumentNavigationSourceIdForTheSameDocument";

  // The second navigation is a same-document navigation for the page loaded by
  // the first navigation. The third navigation is a regular navigation. The
  // fourth navigation goes back.
  //
  // The back/forward navigation (#4) goes back to the navigation entry created
  // by the same-document navigation (#2), so we expect the id corresponding to
  // the previous non-same-document navigation (#1).
  EXPECT_THAT(recorder.GetEntries("HistoryNavigation", {last_navigation_id}),
              testing::ElementsAre(UkmEntry{id4, {{last_navigation_id, id1}}}));
}

IN_PROC_BROWSER_TEST_P(BackForwardCacheMetricsBrowserTest,
                       GoBackToSameDocumentNavigationEntry2) {
  ukm::TestAutoSetUkmRecorder recorder;

  const GURL url1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  const GURL url1_foo(
      embedded_test_server()->GetURL("a.com", "/title1.html#foo"));
  const GURL url2(embedded_test_server()->GetURL("b.com", "/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url1));
  EXPECT_TRUE(NavigateToURL(shell(), url1_foo));
  EXPECT_TRUE(NavigateToURL(shell(), url2));

  web_contents()->GetController().GoToOffset(-2);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  ASSERT_EQ(navigation_ids_.size(), static_cast<size_t>(4));
  ukm::SourceId id1 = ToSourceId(navigation_ids_[0]);
  // ukm::SourceId id2 = ToSourceId(navigation_ids_[1]);
  // ukm::SourceId id3 = ToSourceId(navigation_ids_[2]);
  ukm::SourceId id4 = ToSourceId(navigation_ids_[3]);

  std::string last_navigation_id =
      "LastCommittedCrossDocumentNavigationSourceIdForTheSameDocument";

  // The second navigation is a same-document navigation for the page loaded by
  // the first navigation. The third navigation is a regular navigation. The
  // fourth navigation goes back.
  //
  // The back/forward navigation (#4) goes back to navigation #1, skipping a
  // navigation entry generated by same-document navigation (#2). Ensure that
  // the recorded id belongs to the navigation #1, not #2.
  EXPECT_THAT(recorder.GetEntries("HistoryNavigation", {last_navigation_id}),
              testing::ElementsAre(UkmEntry{id4, {{last_navigation_id, id1}}}));
}

namespace {

struct FeatureUsage {
  ukm::SourceId source_id;
  uint64_t main_frame_features;
  uint64_t same_origin_subframes_features;
  uint64_t cross_origin_subframes_features;
};

bool operator==(const FeatureUsage& lhs, const FeatureUsage& rhs) {
  return lhs.source_id == rhs.source_id &&
         lhs.main_frame_features == rhs.main_frame_features &&
         lhs.same_origin_subframes_features ==
             rhs.same_origin_subframes_features &&
         lhs.cross_origin_subframes_features ==
             rhs.cross_origin_subframes_features;
}

std::ostream& operator<<(std::ostream& os, const FeatureUsage& usage) {
  os << "source_id=" << usage.source_id
     << " main_frame_features=" << usage.main_frame_features
     << " same_origin_features=" << usage.same_origin_subframes_features
     << " cross_origin_features=" << usage.cross_origin_subframes_features;
  return os;
}

std::vector<FeatureUsage> GetFeatureUsageMetrics(
    ukm::TestAutoSetUkmRecorder* recorder) {
  std::vector<FeatureUsage> result;
  for (const auto& entry :
       recorder->GetEntries("HistoryNavigation",
                            {"MainFrameFeatures", "SameOriginSubframesFeatures",
                             "CrossOriginSubframesFeatures"})) {
    FeatureUsage feature_usage;
    feature_usage.source_id = entry.source_id;
    feature_usage.main_frame_features = entry.metrics.at("MainFrameFeatures") &
                                        ~kFeaturesToIgnore.ToEnumBitmask();
    feature_usage.same_origin_subframes_features =
        entry.metrics.at("SameOriginSubframesFeatures") &
        ~kFeaturesToIgnore.ToEnumBitmask();
    feature_usage.cross_origin_subframes_features =
        entry.metrics.at("CrossOriginSubframesFeatures") &
        ~kFeaturesToIgnore.ToEnumBitmask();
    result.push_back(feature_usage);
  }
  return result;
}

}  // namespace

IN_PROC_BROWSER_TEST_P(BackForwardCacheMetricsBrowserTest, Features_MainFrame) {
  ukm::TestAutoSetUkmRecorder recorder;

  const GURL url1(embedded_test_server()->GetURL(
      "/back_forward_cache/page_with_pageshow.html"));
  const GURL url2(embedded_test_server()->GetURL("/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url1));
  EXPECT_TRUE(NavigateToURL(shell(), url2));

  {
    TestNavigationObserver navigation_observer(shell()->web_contents());
    shell()->GoBackOrForward(-1);
    navigation_observer.WaitForNavigationFinished();
  }

  ASSERT_EQ(navigation_ids_.size(), static_cast<size_t>(3));
  // ukm::SourceId id1 = ToSourceId(navigation_ids_[0]);
  // ukm::SourceId id2 = ToSourceId(navigation_ids_[1]);
  ukm::SourceId id3 = ToSourceId(navigation_ids_[2]);

  EXPECT_THAT(GetFeatureUsageMetrics(&recorder),
              testing::ElementsAre(FeatureUsage{id3, 0, 0, 0}));
}

IN_PROC_BROWSER_TEST_P(BackForwardCacheMetricsBrowserTest,
                       Features_MainFrame_CrossOriginNavigation) {
  ukm::TestAutoSetUkmRecorder recorder;

  const GURL url1(embedded_test_server()->GetURL(
      "/back_forward_cache/page_with_pageshow.html"));
  const GURL url2(embedded_test_server()->GetURL("bar.com", "/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url1));
  EXPECT_TRUE(NavigateToURL(shell(), url2));

  {
    TestNavigationObserver navigation_observer(shell()->web_contents());
    shell()->GoBackOrForward(-1);
    navigation_observer.WaitForNavigationFinished();
  }

  ASSERT_EQ(navigation_ids_.size(), static_cast<size_t>(3));
  // ukm::SourceId id1 = ToSourceId(navigation_ids_[0]);
  // ukm::SourceId id2 = ToSourceId(navigation_ids_[1]);
  ukm::SourceId id3 = ToSourceId(navigation_ids_[2]);

  EXPECT_THAT(GetFeatureUsageMetrics(&recorder),
              testing::ElementsAre(FeatureUsage{id3, 0, 0, 0}));
}

IN_PROC_BROWSER_TEST_P(BackForwardCacheMetricsBrowserTest,
                       Features_SameOriginSubframes) {
  ukm::TestAutoSetUkmRecorder recorder;

  const GURL url1(embedded_test_server()->GetURL(
      "/back_forward_cache/page_with_same_origin_subframe_with_pageshow.html"));
  const GURL url2(embedded_test_server()->GetURL("b.com", "/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url1));
  EXPECT_TRUE(NavigateToURL(shell(), url2));

  {
    // We are waiting for two navigations here: main frame and subframe.
    // However, when back/forward cache is enabled, back navigation to a page
    // with subframes will not trigger a subframe navigation (since the
    // subframe is cached with the page).
    TestNavigationObserver navigation_observer(
        shell()->web_contents(), 1 + (IsBackForwardCacheEnabled() ? 0 : 1));
    shell()->GoBackOrForward(-1);
    navigation_observer.WaitForNavigationFinished();
  }

  ASSERT_EQ(navigation_ids_.size(),
            static_cast<size_t>(4 + (IsBackForwardCacheEnabled() ? 0 : 1)));
  // ukm::SourceId id1 = ToSourceId(navigation_ids_[0]);
  // ukm::SourceId id2 = ToSourceId(navigation_ids_[1]);
  // ukm::SourceId id3 = ToSourceId(navigation_ids_[2]);
  ukm::SourceId id4 = ToSourceId(navigation_ids_[3]);

  EXPECT_THAT(GetFeatureUsageMetrics(&recorder),
              testing::ElementsAre(FeatureUsage{id4, 0, 0, 0}));
}

IN_PROC_BROWSER_TEST_P(BackForwardCacheMetricsBrowserTest,
                       Features_SameOriginSubframes_CrossOriginNavigation) {
  ukm::TestAutoSetUkmRecorder recorder;

  const GURL url1(embedded_test_server()->GetURL(
      "/back_forward_cache/page_with_same_origin_subframe_with_pageshow.html"));
  const GURL url2(embedded_test_server()->GetURL("bar.com", "/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url1));
  EXPECT_TRUE(NavigateToURL(shell(), url2));

  ASSERT_TRUE(HistoryGoBack(web_contents()));

  // When the page is restored from back/forward cache, there is one navigation
  // corresponding to the bfcache restore. Whereas, when the page is reloaded,
  // there are two navigations i.e., one loading the main frame and one loading
  // the subframe.
  ASSERT_EQ(navigation_ids_.size(), IsBackForwardCacheEnabled() ? 4u : 5u);
  // ukm::SourceId id1 = ToSourceId(navigation_ids_[0]);
  // ukm::SourceId id2 = ToSourceId(navigation_ids_[1]);
  // ukm::SourceId id3 = ToSourceId(navigation_ids_[2]);
  ukm::SourceId id4 = ToSourceId(navigation_ids_[3]);

  EXPECT_THAT(GetFeatureUsageMetrics(&recorder),
              testing::ElementsAre(FeatureUsage{id4, 0, 0, 0}));
}

IN_PROC_BROWSER_TEST_P(BackForwardCacheMetricsBrowserTest,
                       Features_CrossOriginSubframes) {
  ukm::TestAutoSetUkmRecorder recorder;

  const GURL url1(embedded_test_server()->GetURL(
      "/back_forward_cache/"
      "page_with_cross_origin_subframe_with_pageshow.html"));
  const GURL url2(embedded_test_server()->GetURL("b.com", "/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url1));
  EXPECT_TRUE(NavigateToURL(shell(), url2));

  {
    // We are waiting for two navigations here: main frame and subframe.
    // However, when back/forward cache is enabled, back navigation to a page
    // with subframes will not trigger a subframe navigation (since the
    // subframe is cached with the page).
    TestNavigationObserver navigation_observer(
        shell()->web_contents(), 1 + (IsBackForwardCacheEnabled() ? 0 : 1));
    shell()->GoBackOrForward(-1);
    navigation_observer.WaitForNavigationFinished();
  }

  ASSERT_EQ(navigation_ids_.size(),
            static_cast<size_t>(4 + (IsBackForwardCacheEnabled() ? 0 : 1)));
  // ukm::SourceId id1 = ToSourceId(navigation_ids_[0]);
  // ukm::SourceId id2 = ToSourceId(navigation_ids_[1]);
  // ukm::SourceId id3 = ToSourceId(navigation_ids_[2]);
  ukm::SourceId id4 = ToSourceId(navigation_ids_[3]);

  EXPECT_THAT(GetFeatureUsageMetrics(&recorder),
              testing::ElementsAre(FeatureUsage{id4, 0, 0, 0}));
}

IN_PROC_BROWSER_TEST_P(BackForwardCacheMetricsBrowserTest, DedicatedWorker) {
  // This test should only run if the feature is disabled.
  if (base::FeatureList::IsEnabled(
          blink::features::kBackForwardCacheDedicatedWorker))
    return;

  ukm::TestAutoSetUkmRecorder recorder;
  const GURL url(embedded_test_server()->GetURL(
      "/back_forward_cache/page_with_dedicated_worker.html"));

  NavigateAndWaitForDisablingFeature(
      url,
      blink::scheduler::WebSchedulerTrackedFeature::kDedicatedWorkerOrWorklet);
}

// TODO(https://crbug.com/154571): Shared workers are not available on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_SharedWorker DISABLED_SharedWorker
#else
#define MAYBE_SharedWorker SharedWorker
#endif
IN_PROC_BROWSER_TEST_P(BackForwardCacheMetricsBrowserTest, MAYBE_SharedWorker) {
  const GURL url(embedded_test_server()->GetURL(
      "/back_forward_cache/page_with_shared_worker.html"));

  NavigateAndWaitForDisablingFeature(
      url, blink::scheduler::WebSchedulerTrackedFeature::kSharedWorker);
}

IN_PROC_BROWSER_TEST_P(BackForwardCacheMetricsBrowserTest, Geolocation) {
  const GURL url1(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url1));

  RenderFrameHostImpl* main_frame = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame());
  EXPECT_EQ("success", EvalJs(main_frame, R"(
    new Promise(resolve => {
      navigator.geolocation.getCurrentPosition(
        resolve.bind(this, "success"),
        resolve.bind(this, "failure"))
      });
  )"));
}

class RecordBackForwardCacheMetricsWithoutEnabling
    : public BackForwardCacheMetricsBrowserTestBase {
 public:
  RecordBackForwardCacheMetricsWithoutEnabling() {
    // Sets the allowed websites for testing.
    std::string allowed_websites =
        "https://a.allowed/back_forward_cache/, "
        "https://b.allowed/";
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{kRecordBackForwardCacheMetricsWithoutEnabling,
          {{"allowed_websites", allowed_websites}}}},
        // Disable BackForwardCacheMemoryControl to only allow URLs passed via
        // params for all devices irrespective of their memory. As when
        // DeviceHasEnoughMemoryForBackForwardCache() is false, we allow all
        // URLs as default.
        {features::kBackForwardCache,
         features::kBackForwardCacheMemoryControls});
  }

  ~RecordBackForwardCacheMetricsWithoutEnabling() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(RecordBackForwardCacheMetricsWithoutEnabling,
                       ReloadsAndHistoryNavigations) {
  base::HistogramTester histogram_tester;
  using ReloadsAndHistoryNavigations =
      BackForwardCacheMetrics::ReloadsAndHistoryNavigations;
  using ReloadsAfterHistoryNavigation =
      BackForwardCacheMetrics::ReloadsAfterHistoryNavigation;

  const char kReloadsAndHistoryNavigationsHistogram[] =
      "BackForwardCache.ReloadsAndHistoryNavigations";
  const char kReloadsAfterHistoryNavigationHistogram[] =
      "BackForwardCache.ReloadsAfterHistoryNavigation";

  EXPECT_THAT(
      histogram_tester.GetAllSamples(kReloadsAndHistoryNavigationsHistogram),
      testing::IsEmpty());
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kReloadsAfterHistoryNavigationHistogram),
      testing::IsEmpty());

  GURL url1(embedded_test_server()->GetURL(
      "a.allowed", "/back_forward_cache/allowed_path.html"));
  GURL url2(embedded_test_server()->GetURL(
      "b.disallowed", "/back_forward_cache/disallowed_path.html"));

  // 1) Navigate to url1.
  EXPECT_TRUE(NavigateToURL(shell(), url1));
  // 2) Navigate to url2.
  EXPECT_TRUE(NavigateToURL(shell(), url2));

  // 3) Go back to url1 and reload.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  web_contents()->GetController().Reload(content::ReloadType::NORMAL, false);
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  // Bucket count both "kHistoryNavigation" and
  // "kReloadAfterHistoryNavigation" should be 1 after one history
  // navigation and one reload.
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kReloadsAndHistoryNavigationsHistogram),
      ElementsAre(
          Bucket(static_cast<int>(
                     ReloadsAndHistoryNavigations::kHistoryNavigation),
                 1),
          Bucket(
              static_cast<int>(
                  ReloadsAndHistoryNavigations::kReloadAfterHistoryNavigation),
              1)));

  // BackForwardCache is disabled here, the navigation is not served from
  // back/forward cache.
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kReloadsAfterHistoryNavigationHistogram),
      ElementsAre(Bucket(
          static_cast<int>(
              ReloadsAfterHistoryNavigation::kNotServedFromBackForwardCache),
          1)));

  // 4) Go forward to url2 and reload.
  web_contents()->GetController().GoForward();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  web_contents()->GetController().Reload(content::ReloadType::NORMAL, false);
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  // Bucket count both "kHistoryNavigation" and
  // "kReloadAfterHistoryNavigation" should still be 1 since the url2 is
  // not in the list of allowed_websties by
  // "kRecordBackForwardCacheMetricsWithoutEnabling" feature.
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kReloadsAndHistoryNavigationsHistogram),
      ElementsAre(
          Bucket(static_cast<int>(
                     ReloadsAndHistoryNavigations::kHistoryNavigation),
                 1),
          Bucket(
              static_cast<int>(
                  ReloadsAndHistoryNavigations::kReloadAfterHistoryNavigation),
              1)));

  EXPECT_THAT(
      histogram_tester.GetAllSamples(kReloadsAfterHistoryNavigationHistogram),
      ElementsAre(Bucket(
          static_cast<int>(
              ReloadsAfterHistoryNavigation::kNotServedFromBackForwardCache),
          1)));
}

IN_PROC_BROWSER_TEST_P(BackForwardCacheMetricsBrowserTest,
                       RecordReloadsAfterHistoryNavigation) {
  if (!IsBackForwardCacheEnabled())
    return;
  base::HistogramTester histogram_tester;
  using ReloadsAfterHistoryNavigation =
      BackForwardCacheMetrics::ReloadsAfterHistoryNavigation;
  const char kReloadsAfterHistoryNavigationHistogram[] =
      "BackForwardCache.ReloadsAfterHistoryNavigation";

  GURL url1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url2(embedded_test_server()->GetURL("b.com", "/title2.html"));
  GURL url3(embedded_test_server()->GetURL("c.com", "/title3.html"));

  // 1) Navigate to url1 and reload.
  EXPECT_TRUE(NavigateToURL(shell(), url1));
  web_contents()->GetController().Reload(content::ReloadType::NORMAL, false);
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  // No reloads should be recorded since it is not a history navigation.
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kReloadsAfterHistoryNavigationHistogram),
      testing::IsEmpty());

  // 2) Navigate to url2.
  EXPECT_TRUE(NavigateToURL(shell(), url2));

  // 3) Go back to url1 and reload.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  web_contents()->GetController().Reload(content::ReloadType::NORMAL, false);
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  // Bucket with "kServedFromBackForwardCache" should be 1 as url1 is served
  // from cache.
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kReloadsAfterHistoryNavigationHistogram),
      ElementsAre(Bucket(
          static_cast<int>(
              ReloadsAfterHistoryNavigation::kServedFromBackForwardCache),
          1)));

  // 4) Go forward to url2 and reload twice.
  web_contents()->GetController().GoForward();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  web_contents()->GetController().Reload(content::ReloadType::NORMAL, false);
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  web_contents()->GetController().Reload(content::ReloadType::NORMAL, false);
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  // Bucket with "kServedFromBackForwardCache" should be 2 as only the first
  // reload after history navigation is recorded.
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kReloadsAfterHistoryNavigationHistogram),
      ElementsAre(Bucket(
          static_cast<int>(
              ReloadsAfterHistoryNavigation::kServedFromBackForwardCache),
          2)));

  // 5) Go back and navigate to url3.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  RenderFrameHostImpl* rfh_url1 =
      web_contents()->GetPrimaryFrameTree().root()->current_frame_host();

  // Make url1 ineligible for caching so that when we navigate back it doesn't
  // fetch the RenderFrameHost from the back/forward cache.
  DisableBFCacheForRFHForTesting(rfh_url1);
  EXPECT_TRUE(NavigateToURL(shell(), url3));

  // 6) Go back and reload.
  // Ensure that "not served" is recorded.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  web_contents()->GetController().Reload(content::ReloadType::NORMAL, false);
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  // Bucket with "kNotServedFromBackForwardCache" should be 1 as url3 is not
  // served from BackForwardCache.
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kReloadsAfterHistoryNavigationHistogram),
      ElementsAre(
          Bucket(static_cast<int>(ReloadsAfterHistoryNavigation::
                                      kNotServedFromBackForwardCache),
                 1),
          Bucket(
              static_cast<int>(
                  ReloadsAfterHistoryNavigation::kServedFromBackForwardCache),
              2)));
}

IN_PROC_BROWSER_TEST_P(BackForwardCacheMetricsBrowserTest,
                       RestoreNavigationToNextPaint) {
  if (!IsBackForwardCacheEnabled())
    return;
  base::HistogramTester histogram_tester;
  const char kRestoreNavigationToNextPaintTimeHistogram[] =
      "BackForwardCache.Restore.NavigationToFirstPaint";

  GURL url1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url2(embedded_test_server()->GetURL("b.com", "/title2.html"));

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  kRestoreNavigationToNextPaintTimeHistogram),
              testing::IsEmpty());

  // 1) Navigate to url1.
  EXPECT_TRUE(NavigateToURL(shell(), url1));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate to url2.
  EXPECT_TRUE(NavigateToURL(shell(), url2));
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 3) Go back to url1 and check if the metrics are recorded. Make sure the
  // page is restored from cache.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_EQ(rfh_a, current_frame_host());
  EXPECT_EQ(shell()->web_contents()->GetVisibility(), Visibility::VISIBLE);

  // Verify if the NavigationToFirstPaint metric was recorded on restore.
  do {
    FetchHistogramsFromChildProcesses();
    GiveItSomeTime();
  } while (
      histogram_tester.GetAllSamples(kRestoreNavigationToNextPaintTimeHistogram)
          .empty());
}

class BackForwardCacheMetricsPrerenderingBrowserTest
    : public BackForwardCacheMetricsBrowserTest {
 public:
  BackForwardCacheMetricsPrerenderingBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &BackForwardCacheMetricsPrerenderingBrowserTest::web_contents,
            base::Unretained(this))) {}
  ~BackForwardCacheMetricsPrerenderingBrowserTest() override = default;

  test::PrerenderTestHelper* prerender_helper() { return &prerender_helper_; }

  WebContents* web_contents() { return shell()->web_contents(); }

 private:
  test::PrerenderTestHelper prerender_helper_;
};

// Tests that activating a prerender works correctly when navigated
// back.
IN_PROC_BROWSER_TEST_P(BackForwardCacheMetricsPrerenderingBrowserTest,
                       MainFrameNavigation) {
  ukm::TestAutoSetUkmRecorder recorder;

  const GURL url1(embedded_test_server()->GetURL(
      "/back_forward_cache/page_with_pageshow.html"));
  const GURL prerender_url(embedded_test_server()->GetURL("/title1.html"));
  const GURL url2(embedded_test_server()->GetURL("/title2.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url1));

  // Loads a page in the prerender.
  int host_id = prerender_helper()->AddPrerender(prerender_url);
  test::PrerenderHostObserver host_observer(*web_contents(), host_id);
  RenderFrameHost* prerender_rfh =
      prerender_helper()->GetPrerenderedMainFrameHost(host_id);
  EXPECT_TRUE(WaitForRenderFrameReady(prerender_rfh));

  // Activates the page from the prerendering.
  prerender_helper()->NavigatePrimaryPage(prerender_url);
  // Makes sure that the page is activated from the prerendering.
  EXPECT_TRUE(host_observer.was_activated());
  EXPECT_TRUE(WaitForRenderFrameReady(web_contents()->GetPrimaryMainFrame()));

  EXPECT_TRUE(NavigateToURL(shell(), url2));

  {
    TestNavigationObserver navigation_observer(shell()->web_contents());
    shell()->GoBackOrForward(-1);
    navigation_observer.WaitForNavigationFinished();
  }

  // The navigations observed should be:
  // 1) url1
  // 2) prerender_url (prerender)
  // 3) prerender_url (activate prerender)
  // 4) url2
  // 5) prerender_url (back navigation)

  ASSERT_EQ(navigation_ids_.size(), static_cast<size_t>(5));
  ukm::SourceId id5 = ToSourceId(navigation_ids_[4]);

  // We should only record metrics for the last navigation.
  EXPECT_THAT(GetFeatureUsageMetrics(&recorder),
              testing::ElementsAre(FeatureUsage{id5, 0, 0, 0}));
}

class BackForwardCacheMetricsFencedFrameBrowserTest
    : public BackForwardCacheMetricsBrowserTest {
 public:
  BackForwardCacheMetricsFencedFrameBrowserTest() = default;
  ~BackForwardCacheMetricsFencedFrameBrowserTest() override = default;

  test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_test_helper_;
  }

 private:
  test::FencedFrameTestHelper fenced_frame_test_helper_;
};

// Tests that fenced frame navigation doesn't have BackForwardCacheMetrics.
IN_PROC_BROWSER_TEST_P(BackForwardCacheMetricsFencedFrameBrowserTest,
                       FenceFrameNavigation) {
  ukm::TestAutoSetUkmRecorder recorder;

  const GURL url1(embedded_test_server()->GetURL("/title1.html"));
  const GURL fenced_frame_url1(
      embedded_test_server()->GetURL("/fenced_frames/title1.html"));
  const GURL url2(embedded_test_server()->GetURL("/title2.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url1));

  auto* fenced_frame = fenced_frame_test_helper().CreateFencedFrame(
      web_contents()->GetPrimaryMainFrame(), fenced_frame_url1);
  NavigationEntryImpl* fenced_frame_entry = FrameTreeNode::From(fenced_frame)
                                                ->frame_tree()
                                                .controller()
                                                .GetLastCommittedEntry();
  EXPECT_EQ(fenced_frame_entry->back_forward_cache_metrics(), nullptr);

  EXPECT_TRUE(NavigateToURL(shell(), url2));
  // Navigate back to `url1`.
  ASSERT_TRUE(HistoryGoBack(shell()->web_contents()));

  // The navigations observed should be:
  // 1) url1
  // 2) fenced_frame_url1
  // 3) url2
  // 4) url1 (back navigation)

  ASSERT_EQ(navigation_ids_.size(), static_cast<size_t>(4));
  ukm::SourceId id4 = ToSourceId(navigation_ids_[3]);

  // We should only record metrics for the last navigation.
  EXPECT_THAT(GetFeatureUsageMetrics(&recorder),
              testing::ElementsAre(FeatureUsage{id4, 0, 0, 0}));
}

INSTANTIATE_TEST_SUITE_P(All,
                         BackForwardCacheMetricsBrowserTest,
                         testing::ValuesIn({BackForwardCacheStatus::kDisabled,
                                            BackForwardCacheStatus::kEnabled}),
                         BackForwardCacheMetricsBrowserTest::DescribeParams);
INSTANTIATE_TEST_SUITE_P(All,
                         BackForwardCacheMetricsPrerenderingBrowserTest,
                         testing::ValuesIn({BackForwardCacheStatus::kDisabled,
                                            BackForwardCacheStatus::kEnabled}),
                         BackForwardCacheMetricsBrowserTest::DescribeParams);

INSTANTIATE_TEST_SUITE_P(All,
                         BackForwardCacheMetricsFencedFrameBrowserTest,
                         testing::ValuesIn({BackForwardCacheStatus::kDisabled,
                                            BackForwardCacheStatus::kEnabled}),
                         BackForwardCacheMetricsBrowserTest::DescribeParams);

}  // namespace content
