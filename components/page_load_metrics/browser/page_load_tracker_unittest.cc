// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/page_load_tracker.h"

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "components/page_load_metrics/browser/observers/page_load_metrics_observer_content_test_harness.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "net/base/load_timing_info.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace page_load_metrics {

namespace {

const char kTestUrl[] = "https://a.test/";

struct PageLoadMetricsObserverEvents final {
  size_t ready_to_commit_next_navigation_count = 0;
  bool was_started = false;
  bool was_fenced_frames_started = false;
  bool was_prerender_started = false;
  bool was_committed = false;
  size_t render_frame_deleted_count = 0;
  size_t sub_frame_deleted_count = 0;
  bool was_prerendered_page_activated = false;
  size_t sub_frame_navigation_count = 0;
};

using content::test::ScopedPrerenderWebContentsDelegate;

class TestPageLoadMetricsObserver final : public PageLoadMetricsObserver {
 public:
  TestPageLoadMetricsObserver(PageLoadMetricsObserverEvents* events)
      : events_(events) {}

  void StopObservingOnPrerender() { stop_on_prerender_ = true; }
  void StopObservingOnFencedFrames() { stop_on_fenced_frames_ = true; }

 private:
  void ReadyToCommitNextNavigation(
      content::NavigationHandle* navigation_handle) override {
    events_->ready_to_commit_next_navigation_count++;
  }
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override {
    EXPECT_FALSE(events_->was_started);
    events_->was_started = true;
    return CONTINUE_OBSERVING;
  }
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override {
    EXPECT_FALSE(events_->was_fenced_frames_started);
    events_->was_fenced_frames_started = true;
    return stop_on_fenced_frames_ ? STOP_OBSERVING : CONTINUE_OBSERVING;
  }
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override {
    EXPECT_FALSE(events_->was_prerender_started);
    events_->was_prerender_started = true;
    return stop_on_prerender_ ? STOP_OBSERVING : CONTINUE_OBSERVING;
  }
  ObservePolicy OnCommit(
      content::NavigationHandle* navigation_handle) override {
    EXPECT_FALSE(events_->was_committed);
    events_->was_committed = true;
    return CONTINUE_OBSERVING;
  }
  void OnDidFinishSubFrameNavigation(
      content::NavigationHandle* navigation_handle) override {
    events_->sub_frame_navigation_count++;
  }
  void OnRenderFrameDeleted(content::RenderFrameHost* rfh) override {
    events_->render_frame_deleted_count++;
  }
  void OnSubFrameDeleted(int frame_tree_node_id) override {
    events_->sub_frame_deleted_count++;
  }
  void DidActivatePrerenderedPage(
      content::NavigationHandle* navigation_handle) override {
    EXPECT_FALSE(events_->was_prerendered_page_activated);
    events_->was_prerendered_page_activated = true;

    EXPECT_NE(ukm::kInvalidSourceId, GetDelegate().GetPageUkmSourceId());
  }

  bool stop_on_prerender_ = false;
  bool stop_on_fenced_frames_ = false;

  // Event records should be owned outside this class as this instance will be
  // automatically destructed on STOP_OBSERVING, and so on.
  raw_ptr<PageLoadMetricsObserverEvents> events_;
};

class PageLoadTrackerTest : public PageLoadMetricsObserverContentTestHarness {
 public:
  PageLoadTrackerTest()
      : observer_(new TestPageLoadMetricsObserver(&events_)) {}

 protected:
  void SetTargetUrl(const std::string& url) { target_url_ = GURL(url); }
  const PageLoadMetricsObserverEvents& GetEvents() const { return events_; }
  ukm::SourceId GetObservedUkmSourceIdFor(const std::string& url) {
    return ukm_source_ids_[url];
  }

  void StopObservingOnPrerender() { observer_->StopObservingOnPrerender(); }
  void StopObservingOnFencedFrames() {
    observer_->StopObservingOnFencedFrames();
  }

 private:
  void RegisterObservers(PageLoadTracker* tracker) override {
    ukm_source_ids_.emplace(tracker->GetUrl().spec(),
                            tracker->GetPageUkmSourceIdForTesting());

    if (tracker->GetUrl() != target_url_)
      return;

    EXPECT_FALSE(is_observer_passed_);
    tracker->AddObserver(
        std::unique_ptr<PageLoadMetricsObserverInterface>(observer_));
    is_observer_passed_ = true;
  }

  base::flat_map<std::string, ukm::SourceId> ukm_source_ids_;

  PageLoadMetricsObserverEvents events_;
  raw_ptr<TestPageLoadMetricsObserver, DanglingUntriaged> observer_;
  bool is_observer_passed_ = false;

  GURL target_url_;
};

TEST_F(PageLoadTrackerTest, PrimaryPageType) {
  ScopedPrerenderWebContentsDelegate web_contents_delegate(*web_contents());

  // Target URL to monitor the tracker via the test observer.
  SetTargetUrl(kTestUrl);

  // Navigate in.
  NavigateAndCommit(GURL(kTestUrl));

  // Check observer behaviors.
  EXPECT_TRUE(GetEvents().was_started);
  EXPECT_FALSE(GetEvents().was_fenced_frames_started);
  EXPECT_FALSE(GetEvents().was_prerender_started);
  EXPECT_TRUE(GetEvents().was_committed);

  // Check metrics.
  tester()->histogram_tester().ExpectUniqueSample(
      internal::kPageLoadTrackerPageType,
      internal::PageLoadTrackerPageType::kPrimaryPage, 1);

  // Navigate out.
  tester()->NavigateToUntrackedUrl();

  // Check observer behaviors.
  EXPECT_EQ(1u, GetEvents().ready_to_commit_next_navigation_count);

  // Check ukm::SourceId.
  EXPECT_NE(ukm::kInvalidSourceId, GetObservedUkmSourceIdFor(kTestUrl));
}

TEST_F(PageLoadTrackerTest, EventForwarding) {
  ScopedPrerenderWebContentsDelegate web_contents_delegate(*web_contents());

  // In the end, we'll construct frame trees as the following:
  //
  //   A     : primary main frame
  //   +- B' : outer dummy node of B
  //
  //   B     : fenced frame
  //   +- C  : iframe

  // Target URL to monitor the tracker via the test observer.
  SetTargetUrl(kTestUrl);
  StopObservingOnFencedFrames();

  // A: Navigate in.
  NavigateAndCommit(GURL(kTestUrl));

  // B: Add and navigate in.
  content::RenderFrameHost* rfh_b =
      content::RenderFrameHostTester::For(web_contents()->GetPrimaryMainFrame())
          ->AppendFencedFrame();
  {
    const char kURL[] = "https://a.test/fenced_frames";
    auto simulator = content::NavigationSimulator::CreateRendererInitiated(
        GURL(kURL), rfh_b);
    ASSERT_NE(nullptr, simulator);
    simulator->Commit();
  }

  // Check observer behaviors.
  // The navigation and frame deletion in the FencedFrames should be observed as
  // sub-frame events.
  EXPECT_TRUE(GetEvents().was_started);
  EXPECT_FALSE(GetEvents().was_fenced_frames_started);
  EXPECT_FALSE(GetEvents().was_prerender_started);
  EXPECT_TRUE(GetEvents().was_committed);
  EXPECT_EQ(1u, GetEvents().sub_frame_navigation_count);
  // MetricsWebContentsObserver::RenderFrameDeleted is called in the first
  // navigation for iframes but not for fenced frames.
  // TODO(https://crbug.com/1301880): Make a reason clear.
  EXPECT_EQ(0u, GetEvents().render_frame_deleted_count);
  EXPECT_EQ(0u, GetEvents().sub_frame_deleted_count);

  // B: Navigate out.
  content::RenderFrameHost* rfh_b_last = nullptr;
  {
    const char kURL[] = "https://b.test/fenced_frames";
    auto simulator = content::NavigationSimulator::CreateRendererInitiated(
        GURL(kURL), rfh_b);
    ASSERT_NE(nullptr, simulator);
    simulator->Commit();

    rfh_b_last = simulator->GetFinalRenderFrameHost();
  }

  // Note that deletion of RenderFrameHost depends on some conditions, e.g. Site
  // Isolation and Back/Forward Cache. In unit tests, NavigationSimulator does
  // delete them when a navigation commits. On Android, the two RenderFrameHost
  // has same SiteInstance and the old one will be not deleted.
#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(0u, GetEvents().render_frame_deleted_count);
#else
  EXPECT_EQ(1u, GetEvents().render_frame_deleted_count);
#endif
  EXPECT_EQ(0u, GetEvents().sub_frame_deleted_count);

  // C: Add and navigate in.
  content::RenderFrameHost* rfh_c =
      content::RenderFrameHostTester::For(rfh_b_last)->AppendChild("c");
  content::RenderFrameHost* rfh_c_last = nullptr;
  {
    const char kURL[] = "https://a.test/iframe";
    auto simulator = content::NavigationSimulator::CreateRendererInitiated(
        GURL(kURL), rfh_c);
    ASSERT_NE(nullptr, simulator);
    simulator->Commit();

    rfh_c_last = simulator->GetFinalRenderFrameHost();
  }

#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(0u, GetEvents().render_frame_deleted_count);
#else
  EXPECT_EQ(2u, GetEvents().render_frame_deleted_count);
#endif
  EXPECT_EQ(0u, GetEvents().sub_frame_deleted_count);

  // Remove C.
  content::RenderFrameHostTester::For(rfh_c_last)->Detach();

#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(1u, GetEvents().render_frame_deleted_count);
#else
  EXPECT_EQ(3u, GetEvents().render_frame_deleted_count);
#endif
  EXPECT_EQ(1u, GetEvents().sub_frame_deleted_count);

  // Remove B.
  content::RenderFrameHostTester::For(rfh_b_last)->Detach();

#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(2u, GetEvents().render_frame_deleted_count);
#else
  EXPECT_EQ(4u, GetEvents().render_frame_deleted_count);
#endif
  // "2" may look good, but it is wrong, indeed. "1" is correct.
  //
  // When deleting a FF root node, methods of MetricsWebContentsObserver will be
  // called in the order RenderFrameDeleted -> FrameDeleted. The former
  // unregister PageLoadTracker corresponding to the given RenderFrameHost. The
  // later shouldn't trigger an event because the target PageLoadTracker has
  // gone already. Although, `sub_frame_deleted_count` is incremented because
  // the logic of MetricsWebContentsObserver::GetPageLoadTracker is wrong: See
  // comment of the method. (This behavior can be observerved by putting printf
  // in PageLoadTracker::FrameTreeNodeDeleted and inspecting forwarding doesn't
  // occur.)
  // TODO(https://crbug.com/1301880): Fix
  // MetricsWebContentsObserver::GetPageLoadTracker
  EXPECT_EQ(2u, GetEvents().sub_frame_deleted_count);
}

TEST_F(PageLoadTrackerTest, PrerenderPageType) {
  ScopedPrerenderWebContentsDelegate web_contents_delegate(*web_contents());

  // Target URL to monitor the tracker via the test observer.
  const char kPrerenderingUrl[] = "https://a.test/prerender";
  SetTargetUrl(kPrerenderingUrl);

  // Navigate in.
  NavigateAndCommit(GURL(kTestUrl));

  // Add a prerender page.
  content::WebContentsTester::For(web_contents())
      ->AddPrerenderAndCommitNavigation(GURL(kPrerenderingUrl));

  // Check observer behaviors.
  EXPECT_FALSE(GetEvents().was_started);
  EXPECT_FALSE(GetEvents().was_fenced_frames_started);
  EXPECT_TRUE(GetEvents().was_prerender_started);
  EXPECT_TRUE(GetEvents().was_committed);

  // Check metrics.
  tester()->histogram_tester().ExpectBucketCount(
      internal::kPageLoadTrackerPageType,
      internal::PageLoadTrackerPageType::kPrimaryPage, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kPageLoadTrackerPageType,
      internal::PageLoadTrackerPageType::kPrerenderPage, 1);

  // Check ukm::SourceId.
  EXPECT_NE(ukm::kInvalidSourceId, GetObservedUkmSourceIdFor(kTestUrl));
  EXPECT_EQ(ukm::kInvalidSourceId, GetObservedUkmSourceIdFor(kPrerenderingUrl));
}

TEST_F(PageLoadTrackerTest, FencedFramesPageType) {
  ScopedPrerenderWebContentsDelegate web_contents_delegate(*web_contents());

  // Target URL to monitor the tracker via the test observer.
  const char kFencedFramesUrl[] = "https://a.test/fenced_frames";
  SetTargetUrl(kFencedFramesUrl);

  // Navigate in.
  NavigateAndCommit(GURL(kTestUrl));

  // Add a fenced frame.
  content::RenderFrameHost* fenced_frame_root =
      content::RenderFrameHostTester::For(web_contents()->GetPrimaryMainFrame())
          ->AppendFencedFrame();
  {
    auto simulator = content::NavigationSimulator::CreateRendererInitiated(
        GURL(kFencedFramesUrl), fenced_frame_root);
    ASSERT_NE(nullptr, simulator);
    simulator->Commit();
  }

  // Check observer
  // behaviors.
  EXPECT_FALSE(GetEvents().was_started);
  EXPECT_TRUE(GetEvents().was_fenced_frames_started);
  EXPECT_FALSE(GetEvents().was_prerender_started);
  EXPECT_TRUE(GetEvents().was_committed);

  // Check metrics.
  tester()->histogram_tester().ExpectBucketCount(
      internal::kPageLoadTrackerPageType,
      internal::PageLoadTrackerPageType::kPrimaryPage, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kPageLoadTrackerPageType,
      internal::PageLoadTrackerPageType::kFencedFramesPage, 1);

  // Check ukm::SourceId.
  EXPECT_NE(ukm::kInvalidSourceId, GetObservedUkmSourceIdFor(kTestUrl));
  EXPECT_EQ(GetObservedUkmSourceIdFor(kTestUrl),
            GetObservedUkmSourceIdFor(kFencedFramesUrl));

  // Navigate out.
  {
    auto simulator = content::NavigationSimulator::CreateRendererInitiated(
        GURL(kTestUrl), fenced_frame_root);
    ASSERT_NE(nullptr, simulator);
    simulator->Commit();
  }

  // Check observer behaviors.
  EXPECT_EQ(1u, GetEvents().ready_to_commit_next_navigation_count);
}

TEST_F(PageLoadTrackerTest, StopObservingOnPrerender) {
  ScopedPrerenderWebContentsDelegate web_contents_delegate(*web_contents());

  // Target URL to monitor the tracker via the test observer.
  const char kPrerenderingUrl[] = "https://a.test/prerender";
  SetTargetUrl(kPrerenderingUrl);
  StopObservingOnPrerender();

  // Navigate in.
  NavigateAndCommit(GURL(kTestUrl));

  // Add a prerender page.
  content::WebContentsTester::For(web_contents())
      ->AddPrerenderAndCommitNavigation(GURL(kPrerenderingUrl));

  // Check observer behaviors.
  EXPECT_FALSE(GetEvents().was_started);
  EXPECT_FALSE(GetEvents().was_fenced_frames_started);
  EXPECT_TRUE(GetEvents().was_prerender_started);
  EXPECT_FALSE(GetEvents().was_committed);
}

TEST_F(PageLoadTrackerTest, StopObservingOnFencedFrames) {
  ScopedPrerenderWebContentsDelegate web_contents_delegate(*web_contents());

  // Target URL to monitor the tracker via the test observer.
  const char kFencedFramesUrl[] = "https://a.test/fenced_frames";
  SetTargetUrl(kFencedFramesUrl);
  StopObservingOnFencedFrames();

  // Navigate in.
  NavigateAndCommit(GURL(kTestUrl));

  // Add a fenced frame.
  content::RenderFrameHost* fenced_frame_root =
      content::RenderFrameHostTester::For(web_contents()->GetPrimaryMainFrame())
          ->AppendFencedFrame();
  auto simulator = content::NavigationSimulator::CreateRendererInitiated(
      GURL(kFencedFramesUrl), fenced_frame_root);
  ASSERT_NE(nullptr, simulator);
  simulator->Commit();

  // Check observer behaviors.
  EXPECT_FALSE(GetEvents().was_started);
  EXPECT_TRUE(GetEvents().was_fenced_frames_started);
  EXPECT_FALSE(GetEvents().was_prerender_started);
  EXPECT_FALSE(GetEvents().was_committed);
}

TEST_F(PageLoadTrackerTest, ResumeOnPrerenderActivation) {
  ScopedPrerenderWebContentsDelegate web_contents_delegate(*web_contents());

  // Target URL to monitor the tracker via the test observer.
  const char kPrerenderingUrl[] = "https://a.test/prerender";
  SetTargetUrl(kPrerenderingUrl);

  // Navigate in.
  NavigateAndCommit(GURL(kTestUrl));

  // Add a prerender page.
  content::WebContentsTester::For(web_contents())
      ->AddPrerenderAndCommitNavigation(GURL(kPrerenderingUrl));

  // Check observer behaviors.
  EXPECT_FALSE(GetEvents().was_started);
  EXPECT_FALSE(GetEvents().was_fenced_frames_started);
  EXPECT_TRUE(GetEvents().was_prerender_started);
  EXPECT_TRUE(GetEvents().was_committed);
  EXPECT_FALSE(GetEvents().was_prerendered_page_activated);

  // Activate the prerendered page.
  content::WebContentsTester::For(web_contents())
      ->ActivatePrerenderedPage(GURL(kPrerenderingUrl));

  EXPECT_TRUE(GetEvents().was_prerendered_page_activated);
}

TEST_F(PageLoadTrackerTest, LargestImageIncorrectLoadTimings) {
  // Construct page load timing to be used in SimulateTimingUpdate.
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::Now();

  timing.paint_timing->largest_contentful_paint->largest_image_load_start =
      base::Milliseconds(56);
  timing.paint_timing->largest_contentful_paint->largest_image_load_end =
      base::Milliseconds(45);
  timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::Milliseconds(34);

  SetTargetUrl(kTestUrl);
  auto navigation_simulator =
      content::NavigationSimulator::CreateBrowserInitiated(GURL(kTestUrl),
                                                           web_contents());
  ASSERT_NE(nullptr, navigation_simulator);

  navigation_simulator->Commit();

  base::TimeTicks reference_time =
      tester()->GetDelegateForCommittedLoad().GetNavigationStart();

  // Construct ExtraRequestCompleteInfo object to be used in
  // SimulateLoadedResource.
  net::LoadTimingInfo load_timing_info;
  load_timing_info.receive_headers_start =
      reference_time + base::Milliseconds(65);

  ExtraRequestCompleteInfo request_info(
      /*final_url=*/url::SchemeHostPort(GURL(kTestUrl)),
      /*remote_endpoint=*/net::IPEndPoint(),
      /*frame_tree_node_id=*/-1, /*was_cached=*/false, /*raw_body_bytes=*/0,
      /*original_network_content_length=*/0,
      /*request_destination=*/network::mojom::RequestDestination::kDocument,
      /*net_error=*/0,
      /*load_timing_info=*/
      std::make_unique<net::LoadTimingInfo>(load_timing_info));

  // Set main frame document receive_headers_start. This field should be set
  // only once.
  const auto request_id = navigation_simulator->GetGlobalRequestID();
  tester()->SimulateLoadedResource(request_info, request_id);

  // Simulate the invocation of PageLoadTracker::OnLoadedResource() again with
  // a ttfb earlier than the image load start and a request destination that is
  // not of type Document. This should not overwrite the
  // receive_headers_start that is already set.
  load_timing_info.receive_headers_start =
      reference_time + base::Milliseconds(29);

  ExtraRequestCompleteInfo request_info1(
      /*final_url=*/url::SchemeHostPort(GURL(kTestUrl)),
      /*remote_endpoint=*/net::IPEndPoint(),
      /*frame_tree_node_id=*/-1, /*was_cached=*/false, /*raw_body_bytes=*/0,
      /*original_network_content_length=*/0,
      /*request_destination=*/network::mojom::RequestDestination::kFrame,
      /*net_error=*/0,
      /*load_timing_info=*/
      std::make_unique<net::LoadTimingInfo>(load_timing_info));

  tester()->SimulateLoadedResource(request_info1, request_id);

  // Set largest contentful paint timings.
  tester()->SimulateTimingUpdate(timing);

  EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                  "PageLoad.PaintTiming.NavigationToLargestContentfulPaint."
                  "ImageLoadStartLessThanDocumentTTFB"),
              testing::ElementsAre(base::Bucket(true, 1)));

  EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                  "PageLoad.PaintTiming.NavigationToLargestContentfulPaint."
                  "ImageLoadEndLessThanLoadStart"),
              testing::ElementsAre(base::Bucket(true, 1)));

  EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                  "PageLoad.PaintTiming.NavigationToLargestContentfulPaint."
                  "ImageLCPLessThanLoadEnd"),
              testing::ElementsAre(base::Bucket(true, 1)));
  EXPECT_THAT(
      tester()->histogram_tester().GetAllSamples(
          "PageLoad.PaintTiming.NavigationToLargestContentfulPaint."
          "ImageLoadStartLessThanDocumentTtfbCauses"),
      testing::ElementsAre(base::Bucket(
          internal::ImageLoadStartLessThanDocumentTtfbCause::kUnknown, 1)));
}

class IrregularLcpPageLoadTrackerTest
    : public PageLoadTrackerTest,
      public testing::WithParamInterface<
          internal::ImageLoadStartLessThanDocumentTtfbCause> {
 public:
  void SetUp() override {
    PageLoadMetricsObserverContentTestHarness::SetUp();
    cause_ = GetParam();
  }
  // Construct page load timing to be used in SimulateTimingUpdate based on the
  // params.
  page_load_metrics::mojom::PageLoadTimingPtr CreatePageLoadTiming() {
    page_load_metrics::mojom::PageLoadTiming timing;
    page_load_metrics::InitPageLoadTimingForTest(&timing);
    timing.navigation_start = base::Time::Now();

    timing.paint_timing->largest_contentful_paint->largest_image_load_start =
        base::Milliseconds(56);
    timing.paint_timing->largest_contentful_paint->largest_image_load_end =
        base::Milliseconds(45);
    timing.paint_timing->largest_contentful_paint->largest_image_paint =
        base::Milliseconds(34);

    switch (cause_) {
      case internal::ImageLoadStartLessThanDocumentTtfbCause::
          kLoadedFromMemoryCache:
        timing.paint_timing->largest_contentful_paint
            ->is_loaded_from_memory_cache = true;
        break;

      case internal::ImageLoadStartLessThanDocumentTtfbCause::
          kPreloadedWithEarlyHints:
        timing.paint_timing->largest_contentful_paint
            ->is_preloaded_with_early_hints = true;
        break;

      case internal::ImageLoadStartLessThanDocumentTtfbCause::
          kLoadedFromMemoryCacheAndPreloadedWithEarlyHints:
        timing.paint_timing->largest_contentful_paint
            ->is_loaded_from_memory_cache = true;
        timing.paint_timing->largest_contentful_paint
            ->is_preloaded_with_early_hints = true;
        break;

      case internal::ImageLoadStartLessThanDocumentTtfbCause::kUnknown:
        break;
    }
    return timing.Clone();
  }

 protected:
  internal::ImageLoadStartLessThanDocumentTtfbCause GetCause() {
    return cause_;
  }

 private:
  internal::ImageLoadStartLessThanDocumentTtfbCause cause_;
};

TEST_P(IrregularLcpPageLoadTrackerTest, LargestImageIncorrectLoadTimingCauses) {
  auto timing = CreatePageLoadTiming();

  SetTargetUrl(kTestUrl);
  auto navigation_simulator =
      content::NavigationSimulator::CreateBrowserInitiated(GURL(kTestUrl),
                                                           web_contents());
  ASSERT_NE(nullptr, navigation_simulator);

  navigation_simulator->Commit();

  // Construct ExtraRequestCompleteInfo object to be used in
  // SimulateLoadedResource.
  base::TimeTicks reference_time =
      tester()->GetDelegateForCommittedLoad().GetNavigationStart();

  net::LoadTimingInfo load_timing_info;
  load_timing_info.receive_headers_start =
      reference_time + base::Milliseconds(65);

  ExtraRequestCompleteInfo request_info(
      /*final_url=*/url::SchemeHostPort(GURL(kTestUrl)),
      /*remote_endpoint=*/net::IPEndPoint(),
      /*frame_tree_node_id=*/-1,
      /*was_cached=*/false,
      /*raw_body_bytes=*/0,
      /*original_network_content_length=*/0,
      /*request_destination=*/network::mojom::RequestDestination::kDocument,
      /*net_error=*/0,
      /*load_timing_info=*/
      std::make_unique<net::LoadTimingInfo>(load_timing_info));

  // Set document receive_headers_start.
  const auto request_id = navigation_simulator->GetGlobalRequestID();
  tester()->SimulateLoadedResource(request_info, request_id);

  // Set largest contentful paint timings.
  tester()->SimulateTimingUpdate(*timing);

  EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                  "PageLoad.PaintTiming.NavigationToLargestContentfulPaint."
                  "ImageLoadStartLessThanDocumentTtfbCauses"),
              testing::ElementsAre(base::Bucket(GetCause(), 1)));
}

std::string PrintImageLoadStartLessThanDocumentTtfbCause(
    ::testing::TestParamInfo<internal::ImageLoadStartLessThanDocumentTtfbCause>
        info) {
  switch (info.param) {
    case internal::ImageLoadStartLessThanDocumentTtfbCause::
        kLoadedFromMemoryCache:
      return "is_loaded_from_memory_cache";

    case internal::ImageLoadStartLessThanDocumentTtfbCause::
        kPreloadedWithEarlyHints:
      return "is_preloaded_with_early_hints";

    case internal::ImageLoadStartLessThanDocumentTtfbCause::
        kLoadedFromMemoryCacheAndPreloadedWithEarlyHints:
      return "is_loaded_from_memory_cache_and_is_preloaded_with_early_hints";

    case internal::ImageLoadStartLessThanDocumentTtfbCause::kUnknown:
      return "unknown";
  }
}

INSTANTIATE_TEST_SUITE_P(
    ,
    IrregularLcpPageLoadTrackerTest,
    ::testing::Values(
        internal::ImageLoadStartLessThanDocumentTtfbCause::
            kLoadedFromMemoryCache,
        internal::ImageLoadStartLessThanDocumentTtfbCause::
            kLoadedFromMemoryCacheAndPreloadedWithEarlyHints,
        internal::ImageLoadStartLessThanDocumentTtfbCause::
            kPreloadedWithEarlyHints,
        internal::ImageLoadStartLessThanDocumentTtfbCause::kUnknown),
    PrintImageLoadStartLessThanDocumentTtfbCause);
}  // namespace

}  // namespace page_load_metrics
