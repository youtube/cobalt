// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/metrics/metrics_collector.h"

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/common/process_type.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace performance_manager {

using ContentType = ProcessNode::ContentType;
using testing::ElementsAre;
using testing::Pair;

const base::TimeDelta kTestMetricsReportDelayTimeout =
    kMetricsReportDelayTimeout + base::Seconds(1);
const char kHtmlMimeType[] = "text/html";

class MetricsCollectorTest : public GraphTestHarness {
 public:
  using Super = GraphTestHarness;

  MetricsCollectorTest() = default;

  MetricsCollectorTest(const MetricsCollectorTest&) = delete;
  MetricsCollectorTest& operator=(const MetricsCollectorTest&) = delete;

  void SetUp() override {
    Super::SetUp();
    auto metrics_collector = std::make_unique<MetricsCollector>();
    metrics_collector_ = metrics_collector.get();
    graph()->PassToGraph(std::move(metrics_collector));
  }

  void TearDown() override {
    graph()->TakeFromGraph(metrics_collector_);  // Destroy the observer.
    metrics_collector_ = nullptr;
    Super::TearDown();
  }

 protected:
  static constexpr uint64_t kDummyID = 1u;

  base::HistogramTester histogram_tester_;

 private:
  raw_ptr<MetricsCollector> metrics_collector_ = nullptr;
};

TEST_F(MetricsCollectorTest, FromBackgroundedToFirstTitleUpdatedUMA) {
  auto page_node = CreateNode<PageNodeImpl>();

  page_node->OnMainFrameNavigationCommitted(
      false, base::TimeTicks::Now(), kDummyID, GURL("http://www.example.org"),
      kHtmlMimeType);
  AdvanceClock(kTestMetricsReportDelayTimeout);

  page_node->SetIsVisible(true);
  page_node->OnTitleUpdated();
  // The page is not backgrounded, thus no metrics recorded.
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstTitleUpdatedUMA,
                                     0);

  page_node->SetIsVisible(false);
  page_node->OnTitleUpdated();
  // The page is backgrounded, thus metrics recorded.
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstTitleUpdatedUMA,
                                     1);
  page_node->OnTitleUpdated();
  // Metrics should only be recorded once per background period, thus metrics
  // not recorded.
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstTitleUpdatedUMA,
                                     1);

  page_node->SetIsVisible(true);
  page_node->SetIsVisible(false);
  page_node->OnTitleUpdated();
  // The page is backgrounded from foregrounded, thus metrics recorded.
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstTitleUpdatedUMA,
                                     2);
}

TEST_F(MetricsCollectorTest,
       FromBackgroundedToFirstTitleUpdatedUMA5MinutesTimeout) {
  auto page_node = CreateNode<PageNodeImpl>();

  page_node->OnMainFrameNavigationCommitted(
      false, base::TimeTicks::Now(), kDummyID, GURL("http://www.example.org"),
      kHtmlMimeType);
  page_node->SetIsVisible(false);
  page_node->OnTitleUpdated();
  // The page is within 5 minutes after main frame navigation was committed,
  // thus no metrics recorded.
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstTitleUpdatedUMA,
                                     0);
  AdvanceClock(kTestMetricsReportDelayTimeout);
  page_node->OnTitleUpdated();
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstTitleUpdatedUMA,
                                     1);
}

TEST_F(MetricsCollectorTest,
       FromBackgroundedToFirstNonPersistentNotificationCreatedUMA) {
  auto process_node = CreateNode<ProcessNodeImpl>();
  auto page_node = CreateNode<PageNodeImpl>();
  auto frame_node = CreateFrameNodeAutoId(process_node.get(), page_node.get());

  page_node->OnMainFrameNavigationCommitted(
      false, base::TimeTicks::Now(), kDummyID, GURL("http://www.example.org"),
      kHtmlMimeType);
  AdvanceClock(kTestMetricsReportDelayTimeout);

  page_node->SetIsVisible(true);
  frame_node->OnNonPersistentNotificationCreated();
  // The page is not backgrounded, thus no metrics recorded.
  histogram_tester_.ExpectTotalCount(
      kTabFromBackgroundedToFirstNonPersistentNotificationCreatedUMA, 0);

  page_node->SetIsVisible(false);
  frame_node->OnNonPersistentNotificationCreated();
  // The page is backgrounded, thus metrics recorded.
  histogram_tester_.ExpectTotalCount(
      kTabFromBackgroundedToFirstNonPersistentNotificationCreatedUMA, 1);
  frame_node->OnNonPersistentNotificationCreated();
  // Metrics should only be recorded once per background period, thus metrics
  // not recorded.
  histogram_tester_.ExpectTotalCount(
      kTabFromBackgroundedToFirstNonPersistentNotificationCreatedUMA, 1);

  page_node->SetIsVisible(true);
  page_node->SetIsVisible(false);
  frame_node->OnNonPersistentNotificationCreated();
  // The page is backgrounded from foregrounded, thus metrics recorded.
  histogram_tester_.ExpectTotalCount(
      kTabFromBackgroundedToFirstNonPersistentNotificationCreatedUMA, 2);
}

TEST_F(
    MetricsCollectorTest,
    FromBackgroundedToFirstNonPersistentNotificationCreatedUMA5MinutesTimeout) {
  auto process_node = CreateNode<ProcessNodeImpl>();
  auto page_node = CreateNode<PageNodeImpl>();
  auto frame_node = CreateFrameNodeAutoId(process_node.get(), page_node.get());

  page_node->OnMainFrameNavigationCommitted(
      false, base::TimeTicks::Now(), kDummyID, GURL("http://www.example.org"),
      kHtmlMimeType);
  page_node->SetIsVisible(false);
  frame_node->OnNonPersistentNotificationCreated();
  // The page is within 5 minutes after main frame navigation was committed,
  // thus no metrics recorded.
  histogram_tester_.ExpectTotalCount(
      kTabFromBackgroundedToFirstNonPersistentNotificationCreatedUMA, 0);
  AdvanceClock(kTestMetricsReportDelayTimeout);
  frame_node->OnNonPersistentNotificationCreated();
  histogram_tester_.ExpectTotalCount(
      kTabFromBackgroundedToFirstNonPersistentNotificationCreatedUMA, 1);
}

TEST_F(MetricsCollectorTest, FromBackgroundedToFirstFaviconUpdatedUMA) {
  auto page_node = CreateNode<PageNodeImpl>();

  page_node->OnMainFrameNavigationCommitted(
      false, base::TimeTicks::Now(), kDummyID, GURL("http://www.example.org"),
      kHtmlMimeType);
  AdvanceClock(kTestMetricsReportDelayTimeout);

  page_node->SetIsVisible(true);
  page_node->OnFaviconUpdated();
  // The page is not backgrounded, thus no metrics recorded.
  histogram_tester_.ExpectTotalCount(
      kTabFromBackgroundedToFirstFaviconUpdatedUMA, 0);

  page_node->SetIsVisible(false);
  page_node->OnFaviconUpdated();
  // The page is backgrounded, thus metrics recorded.
  histogram_tester_.ExpectTotalCount(
      kTabFromBackgroundedToFirstFaviconUpdatedUMA, 1);
  page_node->OnFaviconUpdated();
  // Metrics should only be recorded once per background period, thus metrics
  // not recorded.
  histogram_tester_.ExpectTotalCount(
      kTabFromBackgroundedToFirstFaviconUpdatedUMA, 1);

  page_node->SetIsVisible(true);
  page_node->SetIsVisible(false);
  page_node->OnFaviconUpdated();
  // The page is backgrounded from foregrounded, thus metrics recorded.
  histogram_tester_.ExpectTotalCount(
      kTabFromBackgroundedToFirstFaviconUpdatedUMA, 2);
}

TEST_F(MetricsCollectorTest,
       FromBackgroundedToFirstFaviconUpdatedUMA5MinutesTimeout) {
  auto page_node = CreateNode<PageNodeImpl>();

  page_node->OnMainFrameNavigationCommitted(
      false, base::TimeTicks::Now(), kDummyID, GURL("http://www.example.org"),
      kHtmlMimeType);
  page_node->SetIsVisible(false);
  page_node->OnFaviconUpdated();
  // The page is within 5 minutes after main frame navigation was committed,
  // thus no metrics recorded.
  histogram_tester_.ExpectTotalCount(
      kTabFromBackgroundedToFirstFaviconUpdatedUMA, 0);
  AdvanceClock(kTestMetricsReportDelayTimeout);
  page_node->OnFaviconUpdated();
  histogram_tester_.ExpectTotalCount(
      kTabFromBackgroundedToFirstFaviconUpdatedUMA, 1);
}

TEST_F(MetricsCollectorTest, ProcessLifetime_LaunchAndExit) {
  auto process_node = CreateNode<ProcessNodeImpl>();
  process_node->SetProcess(base::Process::Current(), base::TimeTicks::Now());
  process_node->SetProcessExitStatus(42);
  EXPECT_THAT(
      histogram_tester_.GetTotalCountsForPrefix("Renderer.ProcessLifetime3"),
      ElementsAre(Pair("Renderer.ProcessLifetime3", 1),
                  Pair("Renderer.ProcessLifetime3.Empty", 1)));
}

TEST_F(MetricsCollectorTest, ProcessLifetime_LaunchAndDelete) {
  auto process_node = CreateNode<ProcessNodeImpl>();
  process_node->SetProcess(base::Process::Current(), base::TimeTicks::Now());
  process_node.reset();
  EXPECT_THAT(
      histogram_tester_.GetTotalCountsForPrefix("Renderer.ProcessLifetime3"),
      ElementsAre(Pair("Renderer.ProcessLifetime3", 1),
                  Pair("Renderer.ProcessLifetime3.Empty", 1)));
}

TEST_F(MetricsCollectorTest, ProcessLifetime_ExitWithoutLaunch) {
  auto process_node = CreateNode<ProcessNodeImpl>();
  process_node->SetProcessExitStatus(42);
  EXPECT_THAT(
      histogram_tester_.GetTotalCountsForPrefix("Renderer.ProcessLifetime3"),
      ElementsAre(Pair("Renderer.ProcessLifetime3", 1),
                  Pair("Renderer.ProcessLifetime3.Empty", 1)));
  histogram_tester_.ExpectUniqueTimeSample("Renderer.ProcessLifetime3",
                                           base::TimeDelta(), 1);
  histogram_tester_.ExpectUniqueTimeSample("Renderer.ProcessLifetime3.Empty",
                                           base::TimeDelta(), 1);
}

TEST_F(MetricsCollectorTest, ProcessLifetime_NoLaunchAndNoExit) {
  auto process_node = CreateNode<ProcessNodeImpl>();
  process_node.reset();
  EXPECT_THAT(
      histogram_tester_.GetTotalCountsForPrefix("Renderer.ProcessLifetime3"),
      ElementsAre());
}

TEST_F(MetricsCollectorTest, ProcessLifetime_Extension) {
  auto process_node = CreateNode<ProcessNodeImpl>();
  process_node->add_hosted_content_type(ContentType::kMainFrame);
  process_node->add_hosted_content_type(ContentType::kExtension);
  process_node->SetProcess(base::Process::Current(), base::TimeTicks::Now());
  process_node.reset();
  EXPECT_THAT(
      histogram_tester_.GetTotalCountsForPrefix("Renderer.ProcessLifetime3"),
      ElementsAre(Pair("Renderer.ProcessLifetime3", 1),
                  Pair("Renderer.ProcessLifetime3.Extension", 1)));
}

TEST_F(MetricsCollectorTest, ProcessLifetime_Extension_Mixed) {
  auto process_node = CreateNode<ProcessNodeImpl>();
  process_node->add_hosted_content_type(ContentType::kMainFrame);
  process_node->add_hosted_content_type(ContentType::kExtension);
  process_node->add_hosted_content_type(ContentType::kNavigatedFrame);
  process_node->add_hosted_content_type(ContentType::kSubframe);
  process_node->add_hosted_content_type(ContentType::kExtension);
  process_node->add_hosted_content_type(ContentType::kWorker);
  process_node->SetProcess(base::Process::Current(), base::TimeTicks::Now());
  process_node.reset();
  EXPECT_THAT(
      histogram_tester_.GetTotalCountsForPrefix("Renderer.ProcessLifetime3"),
      ElementsAre(Pair("Renderer.ProcessLifetime3", 1),
                  Pair("Renderer.ProcessLifetime3.Extension", 1)));
}

TEST_F(MetricsCollectorTest, ProcessLifetime_Worker) {
  auto process_node = CreateNode<ProcessNodeImpl>();
  process_node->add_hosted_content_type(ContentType::kWorker);
  process_node->SetProcess(base::Process::Current(), base::TimeTicks::Now());
  process_node.reset();
  EXPECT_THAT(
      histogram_tester_.GetTotalCountsForPrefix("Renderer.ProcessLifetime3"),
      ElementsAre(Pair("Renderer.ProcessLifetime3", 1),
                  Pair("Renderer.ProcessLifetime3.Worker", 1)));
}

TEST_F(MetricsCollectorTest, ProcessLifetime_Speculative) {
  auto process_node = CreateNode<ProcessNodeImpl>();
  process_node->add_hosted_content_type(ContentType::kMainFrame);
  process_node->add_hosted_content_type(ContentType::kSubframe);
  process_node->SetProcess(base::Process::Current(), base::TimeTicks::Now());
  process_node.reset();
  EXPECT_THAT(
      histogram_tester_.GetTotalCountsForPrefix("Renderer.ProcessLifetime3"),
      ElementsAre(Pair("Renderer.ProcessLifetime3", 1),
                  Pair("Renderer.ProcessLifetime3.Speculative", 1)));
}

TEST_F(MetricsCollectorTest, ProcessLifetime_Empty) {
  auto process_node = CreateNode<ProcessNodeImpl>();
  process_node->SetProcess(base::Process::Current(), base::TimeTicks::Now());
  process_node.reset();
  EXPECT_THAT(
      histogram_tester_.GetTotalCountsForPrefix("Renderer.ProcessLifetime3"),
      ElementsAre(Pair("Renderer.ProcessLifetime3", 1),
                  Pair("Renderer.ProcessLifetime3.Empty", 1)));
}

TEST_F(MetricsCollectorTest, ProcessLifetime_MainFrame) {
  auto process_node = CreateNode<ProcessNodeImpl>();
  process_node->add_hosted_content_type(ContentType::kMainFrame);
  process_node->add_hosted_content_type(ContentType::kNavigatedFrame);
  process_node->SetProcess(base::Process::Current(), base::TimeTicks::Now());
  process_node.reset();
  EXPECT_THAT(
      histogram_tester_.GetTotalCountsForPrefix("Renderer.ProcessLifetime3"),
      ElementsAre(Pair("Renderer.ProcessLifetime3", 1),
                  Pair("Renderer.ProcessLifetime3.MainFrame", 1)));
}

TEST_F(MetricsCollectorTest, ProcessLifetime_MainFrame_Mixed) {
  auto process_node = CreateNode<ProcessNodeImpl>();
  process_node->add_hosted_content_type(ContentType::kMainFrame);
  process_node->add_hosted_content_type(ContentType::kNavigatedFrame);
  process_node->add_hosted_content_type(ContentType::kSubframe);
  process_node->add_hosted_content_type(ContentType::kAd);
  process_node->add_hosted_content_type(ContentType::kWorker);
  process_node->SetProcess(base::Process::Current(), base::TimeTicks::Now());
  process_node.reset();
  EXPECT_THAT(
      histogram_tester_.GetTotalCountsForPrefix("Renderer.ProcessLifetime3"),
      ElementsAre(Pair("Renderer.ProcessLifetime3", 1),
                  Pair("Renderer.ProcessLifetime3.MainFrame", 1)));
}

TEST_F(MetricsCollectorTest, ProcessLifetime_Subframe_Ad) {
  auto process_node = CreateNode<ProcessNodeImpl>();
  process_node->add_hosted_content_type(ContentType::kSubframe);
  process_node->add_hosted_content_type(ContentType::kAd);
  process_node->add_hosted_content_type(ContentType::kNavigatedFrame);
  process_node->SetProcess(base::Process::Current(), base::TimeTicks::Now());
  process_node.reset();
  EXPECT_THAT(
      histogram_tester_.GetTotalCountsForPrefix("Renderer.ProcessLifetime3"),
      ElementsAre(Pair("Renderer.ProcessLifetime3", 1),
                  Pair("Renderer.ProcessLifetime3.Subframe_Ad", 1)));
}

TEST_F(MetricsCollectorTest, ProcessLifetime_Subframe_NoAd) {
  auto process_node = CreateNode<ProcessNodeImpl>();
  process_node->add_hosted_content_type(ContentType::kSubframe);
  process_node->add_hosted_content_type(ContentType::kNavigatedFrame);
  process_node->SetProcess(base::Process::Current(), base::TimeTicks::Now());
  process_node.reset();
  EXPECT_THAT(
      histogram_tester_.GetTotalCountsForPrefix("Renderer.ProcessLifetime3"),
      ElementsAre(Pair("Renderer.ProcessLifetime3", 1),
                  Pair("Renderer.ProcessLifetime3.Subframe_NoAd", 1)));
}

TEST_F(MetricsCollectorTest, ProcessLifetime_Subframe_NoAd_Mixed) {
  auto process_node = CreateNode<ProcessNodeImpl>();
  process_node->add_hosted_content_type(ContentType::kSubframe);
  process_node->add_hosted_content_type(ContentType::kNavigatedFrame);
  process_node->add_hosted_content_type(ContentType::kWorker);
  process_node->SetProcess(base::Process::Current(), base::TimeTicks::Now());
  process_node.reset();
  EXPECT_THAT(
      histogram_tester_.GetTotalCountsForPrefix("Renderer.ProcessLifetime3"),
      ElementsAre(Pair("Renderer.ProcessLifetime3", 1),
                  Pair("Renderer.ProcessLifetime3.Subframe_NoAd", 1)));
}

TEST_F(MetricsCollectorTest, ProcessLifetime_Utility) {
  auto process_node =
      CreateNode<ProcessNodeImpl>(content::PROCESS_TYPE_UTILITY);
  process_node->SetProcess(base::Process::Current(), base::TimeTicks::Now());
  process_node.reset();
  EXPECT_THAT(histogram_tester_.GetTotalCountsForPrefix(
                  "ChildProcess.ProcessLifetime.Utility"),
              ElementsAre(Pair("ChildProcess.ProcessLifetime.Utility", 1)));
}

TEST_F(MetricsCollectorTest, NewNavigationWithSameOriginTab) {
  auto page_node = CreateNode<PageNodeImpl>(WebContentsProxy(), "context_1");

  page_node->OnMainFrameNavigationCommitted(
      false, base::TimeTicks::Now(), kDummyID, GURL("http://www.example1.org"),
      kHtmlMimeType);
  EXPECT_THAT(
      histogram_tester_.GetAllSamples("Tabs.NewNavigationWithSameOriginTab"),
      ElementsAre(base::Bucket(0, 1)));

  auto same_origin_page_node =
      CreateNode<PageNodeImpl>(WebContentsProxy(), "context_1");
  same_origin_page_node->OnMainFrameNavigationCommitted(
      false, base::TimeTicks::Now(), kDummyID,
      GURL("http://www.example1.org/example"), kHtmlMimeType);
  EXPECT_THAT(
      histogram_tester_.GetAllSamples("Tabs.NewNavigationWithSameOriginTab"),
      ElementsAre(base::Bucket(0, 1), base::Bucket(1, 1)));

  // Same site navigation under the same page won't be recorded again
  same_origin_page_node->OnMainFrameNavigationCommitted(
      false, base::TimeTicks::Now(), kDummyID + 1,
      GURL("http://www.example1.org/example"), kHtmlMimeType);
  EXPECT_THAT(
      histogram_tester_.GetAllSamples("Tabs.NewNavigationWithSameOriginTab"),
      ElementsAre(base::Bucket(0, 1), base::Bucket(1, 1)));

  auto different_origin_page_node =
      CreateNode<PageNodeImpl>(WebContentsProxy(), "context_1");
  different_origin_page_node->OnMainFrameNavigationCommitted(
      false, base::TimeTicks::Now(), kDummyID, GURL("http://www.example2.org"),
      kHtmlMimeType);
  EXPECT_THAT(
      histogram_tester_.GetAllSamples("Tabs.NewNavigationWithSameOriginTab"),
      ElementsAre(base::Bucket(0, 2), base::Bucket(1, 1)));

  auto different_context_page_node =
      CreateNode<PageNodeImpl>(WebContentsProxy(), "context_2");
  different_context_page_node->OnMainFrameNavigationCommitted(
      false, base::TimeTicks::Now(), kDummyID, GURL("http://www.example1.org"),
      kHtmlMimeType);
  EXPECT_THAT(
      histogram_tester_.GetAllSamples("Tabs.NewNavigationWithSameOriginTab"),
      ElementsAre(base::Bucket(0, 3), base::Bucket(1, 1)));
}

}  // namespace performance_manager
