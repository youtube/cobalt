// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/metrics/quick_insert_performance_metrics.h"

#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/compositor.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

using ::testing::IsEmpty;

void WaitUntilNextFramePresented(ui::Compositor* compositor) {
  base::RunLoop run_loop;
  compositor->RequestSuccessfulPresentationTimeForNextFrame(
      base::BindLambdaForTesting(
          [&](const viz::FrameTimingDetails& frame_timing_details) {
            run_loop.Quit();
          }));
  run_loop.Run();
}

class QuickInsertPerformanceMetricsTest : public views::ViewsTestBase {
 public:
  QuickInsertPerformanceMetricsTest()
      : views::ViewsTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
};

TEST_F(QuickInsertPerformanceMetricsTest,
       DoesNotRecordMetricsWithoutCallingStartRecording) {
  base::HistogramTester histogram;

  QuickInsertPerformanceMetrics metrics(base::TimeTicks::Now());
  metrics.MarkInputFocus();
  metrics.MarkContentsChanged();
  metrics.MarkSearchResultsUpdated(
      QuickInsertPerformanceMetrics::SearchResultsUpdate::kReplace);

  EXPECT_THAT(histogram.GetTotalCountsForPrefix("Ash.Picker.Session"),
              IsEmpty());
}

TEST_F(QuickInsertPerformanceMetricsTest, RecordsFirstFocusLatency) {
  base::HistogramTester histogram;
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);

  const auto trigger_event_timestamp = base::TimeTicks::Now();
  task_environment()->FastForwardBy(base::Seconds(1));
  QuickInsertPerformanceMetrics metrics(trigger_event_timestamp);
  metrics.StartRecording(*widget);
  task_environment()->FastForwardBy(base::Seconds(1));
  metrics.MarkInputFocus();

  histogram.ExpectUniqueTimeSample("Ash.Picker.Session.InputReadyLatency",
                                   base::Seconds(2), 1);
}

TEST_F(QuickInsertPerformanceMetricsTest, RecordsOnlyFirstFocusLatency) {
  base::HistogramTester histogram;
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);

  const auto trigger_event_timestamp = base::TimeTicks::Now();
  task_environment()->FastForwardBy(base::Seconds(1));
  QuickInsertPerformanceMetrics metrics(trigger_event_timestamp);
  metrics.StartRecording(*widget);
  task_environment()->FastForwardBy(base::Seconds(1));
  metrics.MarkInputFocus();
  // Mark a second focus. Only the first focus should be recorded.
  task_environment()->FastForwardBy(base::Seconds(1));
  metrics.MarkInputFocus();

  histogram.ExpectUniqueTimeSample("Ash.Picker.Session.InputReadyLatency",
                                   base::Seconds(2), 1);
}

TEST_F(QuickInsertPerformanceMetricsTest,
       RecordsPresentationLatencyForSearchField) {
  base::HistogramTester histogram;
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);

  QuickInsertPerformanceMetrics metrics(base::TimeTicks::Now());
  metrics.StartRecording(*widget);
  metrics.MarkContentsChanged();
  WaitUntilNextFramePresented(widget->GetCompositor());

  histogram.ExpectTotalCount(
      "Ash.Picker.Session.PresentationLatency.SearchField", 1);
}

TEST_F(QuickInsertPerformanceMetricsTest,
       RecordsPresentationLatencyForResults) {
  base::HistogramTester histogram;
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);

  QuickInsertPerformanceMetrics metrics(base::TimeTicks::Now());
  metrics.StartRecording(*widget);
  metrics.MarkSearchResultsUpdated(
      QuickInsertPerformanceMetrics::SearchResultsUpdate::kReplace);
  WaitUntilNextFramePresented(widget->GetCompositor());

  histogram.ExpectTotalCount(
      "Ash.Picker.Session.PresentationLatency.SearchResults", 1);
}

TEST_F(QuickInsertPerformanceMetricsTest,
       RecordsPresentationLatencyForResultsShowingNoResultsFound) {
  base::HistogramTester histogram;
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);

  QuickInsertPerformanceMetrics metrics(base::TimeTicks::Now());
  metrics.StartRecording(*widget);
  metrics.MarkSearchResultsUpdated(
      QuickInsertPerformanceMetrics::SearchResultsUpdate::kNoResultsFound);
  WaitUntilNextFramePresented(widget->GetCompositor());

  histogram.ExpectTotalCount(
      "Ash.Picker.Session.PresentationLatency.SearchResults", 1);
}

TEST_F(QuickInsertPerformanceMetricsTest,
       RecordsSearchLatencyOnSearchFinished) {
  base::HistogramTester histogram;
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);

  QuickInsertPerformanceMetrics metrics;
  metrics.StartRecording(*widget);
  metrics.MarkContentsChanged();
  task_environment()->FastForwardBy(base::Seconds(1));
  metrics.MarkSearchResultsUpdated(
      QuickInsertPerformanceMetrics::SearchResultsUpdate::kReplace);

  histogram.ExpectUniqueTimeSample("Ash.Picker.Session.SearchLatency",
                                   base::Seconds(1), 1);
}

// TODO: b/349913604 - Replace this metric with a new one which records search
// latency on showing "no results found".
TEST_F(QuickInsertPerformanceMetricsTest,
       DoesNotRecordSearchLatencyOnShowingNoResultsFound) {
  base::HistogramTester histogram;
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);

  QuickInsertPerformanceMetrics metrics;
  metrics.StartRecording(*widget);
  metrics.MarkContentsChanged();
  metrics.MarkSearchResultsUpdated(
      QuickInsertPerformanceMetrics::SearchResultsUpdate::kNoResultsFound);

  histogram.ExpectTotalCount("Ash.Picker.Session.SearchLatency", 0);
}

TEST_F(QuickInsertPerformanceMetricsTest,
       DoesNotRecordSearchLatencyOnCanceledSearch) {
  base::HistogramTester histogram;
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);

  QuickInsertPerformanceMetrics metrics;
  metrics.StartRecording(*widget);
  metrics.MarkContentsChanged();
  task_environment()->FastForwardBy(base::Seconds(1));
  metrics.MarkContentsChanged();
  task_environment()->FastForwardBy(base::Seconds(2));
  metrics.MarkSearchResultsUpdated(
      QuickInsertPerformanceMetrics::SearchResultsUpdate::kReplace);

  histogram.ExpectUniqueTimeSample("Ash.Picker.Session.SearchLatency",
                                   base::Seconds(2), 1);
}

}  // namespace
}  // namespace ash
