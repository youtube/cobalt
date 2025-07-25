// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/predictor_jank_tracker.h"

#include <memory>
#include <string>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_trace_processor.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

class PredictorJankTrackerTest : public testing::Test {
 public:
  PredictorJankTrackerTest() = default;

  void SetUp() override {
    trace_id_ = 0;
    histogram_tester_ = std::make_unique<base::HistogramTester>();
    predictor_jank_tracker_ = std::make_unique<PredictorJankTracker>();
    base_presentation_ts_ = base::TimeTicks::Min();
  }

  void MockFrameProduction(double delta, base::TimeTicks presentation_ts) {
    predictor_jank_tracker_->ReportLatestScrollDelta(
        delta, presentation_ts, vsync_interval,
        EventMetrics::TraceId(++trace_id_));
  }

  int64_t trace_id_ = 0;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  std::unique_ptr<PredictorJankTracker> predictor_jank_tracker_;
  base::TimeTicks base_presentation_ts_;
#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  ::base::test::TracingEnvironment tracing_environment_;
#endif  // BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  const char* missed_fast_histogram_name =
      "Event.Jank.ScrollUpdate.FastScroll.MissedVsync."
      "FrameAboveJankyThreshold2";
  const char* fast_histogram_name =
      "Event.Jank.ScrollUpdate.FastScroll.NoMissedVsync."
      "FrameAboveJankyThreshold2";
  const char* missed_slow_histogram_name =
      "Event.Jank.ScrollUpdate.SlowScroll.MissedVsync."
      "FrameAboveJankyThreshold2";
  const char* slow_histogram_name =
      "Event.Jank.ScrollUpdate.SlowScroll.NoMissedVsync."
      "FrameAboveJankyThreshold2";
  const char* janky_percentage_name =
      "Event.Jank.PredictorJankyFramePercentage2";
  constexpr static base::TimeDelta vsync_interval = base::Milliseconds(16);
};

TEST_F(PredictorJankTrackerTest, BasicNonMissedUpperJankCase) {
  // 50 / 10 = 5.0, > janky threshold and should be reporteed.
  // No dropped frame as frames are separated by 16ms.
  MockFrameProduction(10, base_presentation_ts_);
  MockFrameProduction(50, base_presentation_ts_ + base::Milliseconds(16));
  MockFrameProduction(10, base_presentation_ts_ + base::Milliseconds(32));

  histogram_tester_->ExpectTotalCount(fast_histogram_name, 1);
  // (50 / 10 - 1.2) * 100
  EXPECT_EQ(histogram_tester_->GetTotalSum(fast_histogram_name), 380);
}

TEST_F(PredictorJankTrackerTest, BasicNoMissedSlowUpperJankCase) {
  // 5 / 1 = 5.0, > janky threshold and should be reporteed.
  // No dropped frame as frames are separated by 16ms.
  MockFrameProduction(1, base_presentation_ts_);
  MockFrameProduction(5, base_presentation_ts_ + base::Milliseconds(16));
  MockFrameProduction(1, base_presentation_ts_ + base::Milliseconds(32));

  histogram_tester_->ExpectTotalCount(slow_histogram_name, 1);
  // (5 / 14 - 1.4) * 100
  EXPECT_EQ(histogram_tester_->GetTotalSum(slow_histogram_name), 360);
}

TEST_F(PredictorJankTrackerTest, BasicMissedSlowUpperJankCase) {
  // 5 / 1 = 5.0, > janky threshold and should be reporteed.
  // No dropped frame as frames are separated by 16ms.
  MockFrameProduction(1, base_presentation_ts_);
  MockFrameProduction(5, base_presentation_ts_ + base::Milliseconds(32));
  MockFrameProduction(1, base_presentation_ts_ + base::Milliseconds(48));

  histogram_tester_->ExpectTotalCount(missed_slow_histogram_name, 1);
  // (5 / 1 - 1.4) * 100
  EXPECT_EQ(histogram_tester_->GetTotalSum(missed_slow_histogram_name), 360);
}

TEST_F(PredictorJankTrackerTest, BasicMissedUpperJankCase) {
  // 50 / 10 = 5.0, > janky threshold and should be reporteed.
  // There are dropped frames as the first and second frames are 32 ms apart.
  MockFrameProduction(10, base_presentation_ts_);
  MockFrameProduction(50, base_presentation_ts_ + base::Milliseconds(32));
  MockFrameProduction(10, base_presentation_ts_ + base::Milliseconds(48));

  histogram_tester_->ExpectTotalCount(missed_fast_histogram_name, 1);
  // (50 / 10 - 1.2) * 100
  EXPECT_EQ(histogram_tester_->GetTotalSum(missed_fast_histogram_name), 380);
}

TEST_F(PredictorJankTrackerTest, NegativeNoJankFrame) {
  // [-10, -5, -1] is an increasing non janky sequence.
  MockFrameProduction(-10, base_presentation_ts_);
  MockFrameProduction(-5, base_presentation_ts_ + base::Milliseconds(32));
  MockFrameProduction(-1, base_presentation_ts_ + base::Milliseconds(48));

  histogram_tester_->ExpectTotalCount(missed_fast_histogram_name, 0);
}

TEST_F(PredictorJankTrackerTest, PositiveNoJankFrame) {
  // [10, 5, 1] is a decreasing non janky sequence.
  MockFrameProduction(10, base_presentation_ts_);
  MockFrameProduction(5, base_presentation_ts_ + base::Milliseconds(32));
  MockFrameProduction(1, base_presentation_ts_ + base::Milliseconds(48));

  histogram_tester_->ExpectTotalCount(missed_fast_histogram_name, 0);
}

TEST_F(PredictorJankTrackerTest, BasicNonMissedLowerJankCase) {
  // 50 / 10 = 5.0, > janky threshold and should be reporteed.
  // There are dropped frames as the first and seonc frames are 32 ms apart.
  MockFrameProduction(50, base_presentation_ts_);
  MockFrameProduction(10, base_presentation_ts_ + base::Milliseconds(16));
  MockFrameProduction(50, base_presentation_ts_ + base::Milliseconds(32));

  histogram_tester_->ExpectTotalCount(fast_histogram_name, 1);
  // (50 / 10 - 1.2) * 100
  EXPECT_EQ(histogram_tester_->GetTotalSum(fast_histogram_name), 380);
}

TEST_F(PredictorJankTrackerTest, BasicMissedLowerJankCase) {
  // 50 / 10 = 5.0, > janky threshold and should be reporteed.
  // There are dropped frames as the first and seonc frames are 32 ms apart.
  MockFrameProduction(50, base_presentation_ts_);
  MockFrameProduction(10, base_presentation_ts_ + base::Milliseconds(32));
  MockFrameProduction(50, base_presentation_ts_ + base::Milliseconds(48));

  histogram_tester_->ExpectTotalCount(missed_fast_histogram_name, 1);
  // (50 / 10 - 1.2) * 100
  EXPECT_EQ(histogram_tester_->GetTotalSum(missed_fast_histogram_name), 380);
}

#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
TEST_F(PredictorJankTrackerTest, BasicNonMissedUpperJankCaseWithTracing) {
  base::test::TestTraceProcessor ttp;
  ttp.StartTrace("input.scrolling");
  // 50 / 10 = 5.0, > janky threshold and should be reporteed.
  // No dropped frame as frames are separated by 16ms.
  MockFrameProduction(10, base_presentation_ts_);
  MockFrameProduction(50, base_presentation_ts_ + base::Milliseconds(16));
  MockFrameProduction(11, base_presentation_ts_ + base::Milliseconds(32));

  absl::Status status = ttp.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();
  std::string query =
      R"(
      SELECT
        EXTRACT_ARG(arg_set_id,
          "scroll_predictor_metrics.prev_event_frame_value.delta_value_pixels"
        ) AS prev_delta,
        EXTRACT_ARG(arg_set_id,
          "scroll_predictor_metrics.cur_event_frame_value.delta_value_pixels"
        ) AS cur_delta,
        EXTRACT_ARG(arg_set_id,
          "scroll_predictor_metrics.next_event_frame_value.delta_value_pixels"
        ) AS next_delta
      FROM
        slice
      WHERE
        name = "PredictorJankTracker::ReportJankyFrame"
      )";
  auto result = ttp.RunQuery(query);
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_THAT(result.value(), ::testing::ElementsAre(
                                  std::vector<std::string>{
                                      "prev_delta", "cur_delta", "next_delta"},
                                  std::vector<std::string>{"10", "50", "11"}));
}

#endif  // BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
TEST_F(PredictorJankTrackerTest, NoReportingDirectionChange) {
  // [50, -100, 50] means the user changed their scrolling direction
  // and no predictor performance should be reported.
  MockFrameProduction(50, base_presentation_ts_);
  MockFrameProduction(-100, base_presentation_ts_ + base::Milliseconds(16));
  MockFrameProduction(50, base_presentation_ts_ + base::Milliseconds(32));

  histogram_tester_->ExpectTotalCount(missed_fast_histogram_name, 0);
  histogram_tester_->ExpectTotalCount(fast_histogram_name, 0);
  histogram_tester_->ExpectTotalCount(janky_percentage_name, 0);
}

TEST_F(PredictorJankTrackerTest, JankyFramePercentageEmitted) {
  // a sequence of 50 frames, with 10 irregular jumps means 20%
  // of the frames were janky, and should be reported since frame count
  // is more than 50.
  double pattern[5] = {50, 50, 50, 50, 10};
  for (int i = 1; i <= 64; i++, base_presentation_ts_ += vsync_interval) {
    MockFrameProduction(pattern[i % 5], base_presentation_ts_);
  }

  histogram_tester_->ExpectTotalCount(janky_percentage_name, 1);
  EXPECT_EQ(histogram_tester_->GetTotalSum(janky_percentage_name), 18);
}

TEST_F(PredictorJankTrackerTest, JankyFramePercentageNotEmitted) {
  // a sequence of 49 frames with 20% janky jumps, but the percentage isn't
  // reported because we only report the percentage when more than 50 frames
  // exist in the sequence.
  double pattern[5] = {50, 50, 50, 50, 10};
  for (int i = 1; i <= 63; i++, base_presentation_ts_ += vsync_interval) {
    MockFrameProduction(pattern[i % 5], base_presentation_ts_);
  }
  histogram_tester_->ExpectTotalCount(janky_percentage_name, 0);
}

TEST_F(PredictorJankTrackerTest, JankyFramePercentageEmittedTwice) {
  // Janky frames percentage should be emitted twice, 20% each
  // since we have 100 frames with 20% jank in each scroll.
  double pattern[5] = {50, 50, 50, 50, 10};
  for (int i = 1; i <= 128; i++, base_presentation_ts_ += vsync_interval) {
    MockFrameProduction(pattern[i % 5], base_presentation_ts_);
  }
  histogram_tester_->ExpectTotalCount(janky_percentage_name, 2);
  EXPECT_EQ(histogram_tester_->GetTotalSum(janky_percentage_name), 38);
}

TEST_F(PredictorJankTrackerTest, JankyFramePercentageEmittedWhenReset) {
  // Janky sequence with 20% janky frames, reporting should happen even if
  // the scroll was reset to catch smaller scrolls and residue frames from
  // previous scrolls.
  double pattern[5] = {50, 50, 50, 50, 10};
  for (int i = 1; i <= 64; i++, base_presentation_ts_ += vsync_interval) {
    MockFrameProduction(pattern[i % 5], base_presentation_ts_);
    if (i == 25) {
      predictor_jank_tracker_->ResetCurrentScrollReporting();
    }
  }
  histogram_tester_->ExpectTotalCount(janky_percentage_name, 1);
  EXPECT_EQ(histogram_tester_->GetTotalSum(janky_percentage_name), 18);
}

}  // namespace cc
