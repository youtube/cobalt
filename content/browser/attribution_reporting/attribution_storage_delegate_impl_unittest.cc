// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_storage_delegate_impl.h"

#include <stdint.h>

#include <vector>

#include "base/containers/flat_map.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/attribution_utils.h"
#include "content/browser/attribution_reporting/combinatorics.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"

namespace content {
namespace {

using ::attribution_reporting::mojom::SourceType;
using ::testing::AllOf;
using ::testing::Ge;
using ::testing::Lt;

using FakeReport = ::content::AttributionStorageDelegate::FakeReport;

constexpr base::TimeDelta kDefaultExpiry = base::Days(30);

AttributionReport GetReport(base::Time source_time,
                            base::Time trigger_time,
                            base::TimeDelta expiry = kDefaultExpiry,
                            base::TimeDelta report_window = kDefaultExpiry,
                            SourceType source_type = SourceType::kNavigation) {
  return ReportBuilder(AttributionInfoBuilder().SetTime(trigger_time).Build(),
                       SourceBuilder(source_time)
                           .SetExpiry(expiry)
                           .SetEventReportWindow(report_window)
                           .SetSourceType(source_type)
                           .BuildStored())
      .Build();
}

void RunRandomFakeReportsTest(const SourceType source_type,
                              const int num_stars,
                              const int num_bars,
                              const int num_samples,
                              const double tolerance) {
  const auto source = SourceBuilder()
                          .SetSourceType(source_type)
                          .SetExpiry(kDefaultExpiry)
                          .BuildStored();

  base::flat_map<std::vector<FakeReport>, int> output_counts;
  for (int i = 0; i < num_samples; i++) {
    std::vector<FakeReport> fake_reports =
        AttributionStorageDelegateImpl().GetRandomFakeReports(
            source.common_info(), source.event_report_window_time());
    output_counts[fake_reports]++;
  }

  // This is the coupon collector problem (see
  // https://en.wikipedia.org/wiki/Coupon_collector%27s_problem).
  // For n possible results:
  //
  // the expected number of trials needed to see all possible results is equal
  // to n * Sum_{i = 1,..,n} 1/i.
  //
  // The variance of the number of trials is equal to
  // Sum_{i = 1,.., n} (1 - p_i) / p_i^2,
  // where p_i = (n - i + 1) / n.
  //
  // The probability that t trials are not enough to see all possible results is
  // at most n^{-t/(n*ln(n)) + 1}.
  int expected_num_combinations =
      BinomialCoefficient(num_stars + num_bars, num_stars);
  EXPECT_EQ(static_cast<int>(output_counts.size()), expected_num_combinations);

  // For any of the n possible results, the expected number of times it is seen
  // is equal to 1/n. Moreover, for any possible result, the probability that it
  // is seen more than (1+alpha)*t/n times is at most p_high = exp(- D(1/n +
  // alpha/n || 1/n) * t).
  //
  // The probability that it is seen less than (1-alpha)*t/n times is at most
  // p_low = exp(-D(1/n - alpha/n || 1/n) * t,
  //
  // where D( x || y) = x * ln(x/y) + (1-x) * ln( (1-x) / (1-y) ).
  // See
  // https://en.wikipedia.org/wiki/Chernoff_bound#Additive_form_(absolute_error)
  // for details.
  //
  // Thus, the probability that the number of occurrences of one of the results
  // deviates from its expectation by alpha*t/n is at most
  // n * (p_high + p_low).
  int expected_counts =
      num_samples / static_cast<double>(expected_num_combinations);
  for (const auto& output_count : output_counts) {
    const double abs_error = expected_counts * tolerance;
    EXPECT_NEAR(output_count.second, expected_counts, abs_error);
  }
}

TEST(AttributionStorageDelegateImplTest, ImmediateConversion_FirstWindowUsed) {
  base::Time source_time = base::Time::Now();
  const AttributionReport report =
      GetReport(source_time, /*trigger_time=*/source_time);
  EXPECT_EQ(source_time + base::Days(2) + base::Hours(1),
            AttributionStorageDelegateImpl().GetEventLevelReportTime(
                *report.GetStoredSource(), report.attribution_info().time));
}

TEST(AttributionStorageDelegateImplTest,
     ConversionImmediatelyBeforeWindow_SameWindowUsed) {
  base::Time source_time = base::Time::Now();
  base::Time trigger_time = source_time + base::Days(2) - base::Minutes(1);
  const AttributionReport report = GetReport(source_time, trigger_time);
  EXPECT_EQ(source_time + base::Days(2) + base::Hours(1),
            AttributionStorageDelegateImpl().GetEventLevelReportTime(
                *report.GetStoredSource(), report.attribution_info().time));
}

TEST(AttributionStorageDelegateImplTest,
     ConversionImmediatelyAfterWindow_NextWindowUsed) {
  base::Time source_time = base::Time::Now();

  // The deadline for a window is 1 hour before the window. Use a time just
  // after the deadline.
  base::Time trigger_time = source_time + base::Days(2) + base::Minutes(1);
  const AttributionReport report = GetReport(source_time, trigger_time);
  EXPECT_EQ(source_time + base::Days(7) + base::Hours(1),
            AttributionStorageDelegateImpl().GetEventLevelReportTime(
                *report.GetStoredSource(), report.attribution_info().time));
}

TEST(AttributionStorageDelegateImplTest,
     ImpressionExpiryBeforeTwoDayWindow_ExpiryUsed) {
  base::Time source_time = base::Time::Now();
  base::Time trigger_time = source_time + base::Hours(1);

  // Set the impression to expire before the two day window.
  const AttributionReport report = GetReport(source_time, trigger_time,
                                             /*expiry=*/base::Hours(2));
  EXPECT_EQ(source_time + base::Hours(3),
            AttributionStorageDelegateImpl().GetEventLevelReportTime(
                *report.GetStoredSource(), report.attribution_info().time));
}

TEST(AttributionStorageDelegateImplTest,
     ImpressionExpiryBeforeSevenDayWindow_ExpiryWindowUsed) {
  base::Time source_time = base::Time::Now();
  base::Time trigger_time = source_time + base::Days(3);

  // Set the impression to expire before the two day window.
  const AttributionReport report = GetReport(source_time, trigger_time,
                                             /*expiry=*/base::Days(4));

  EXPECT_EQ(source_time + base::Days(4) + base::Hours(1),
            AttributionStorageDelegateImpl().GetEventLevelReportTime(
                *report.GetStoredSource(), report.attribution_info().time));
}

TEST(AttributionStorageDelegateImplTest,
     ImpressionExpiryAfterSevenDayWindow_ExpiryWindowUsed) {
  base::Time source_time = base::Time::Now();
  base::Time trigger_time = source_time + base::Days(7) + base::Hours(1);

  // Set the impression to expire before the two day window.
  const AttributionReport report = GetReport(source_time, trigger_time,
                                             /*expiry=*/base::Days(9));

  // The expiry window is reported one hour after expiry time.
  EXPECT_EQ(source_time + base::Days(9) + base::Hours(1),
            AttributionStorageDelegateImpl().GetEventLevelReportTime(
                *report.GetStoredSource(), report.attribution_info().time));
}

TEST(AttributionStorageDelegateImplTest,
     SourceTypeEvent_ExpiryLessThanTwoDays_ExpiryUsed) {
  base::Time source_time = base::Time::Now();
  base::Time trigger_time = source_time + base::Days(3);
  const AttributionReport report =
      GetReport(source_time, trigger_time,
                /*expiry=*/base::Days(1),
                /*report_window=*/base::Days(1), SourceType::kEvent);
  EXPECT_EQ(source_time + base::Days(1) + base::Hours(1),
            AttributionStorageDelegateImpl().GetEventLevelReportTime(
                *report.GetStoredSource(), report.attribution_info().time));
}

TEST(AttributionStorageDelegateImplTest,
     SourceTypeEvent_ExpiryGreaterThanTwoDays_ExpiryUsed) {
  base::Time source_time = base::Time::Now();
  base::Time trigger_time = source_time + base::Days(3);
  const AttributionReport report =
      GetReport(source_time, trigger_time,
                /*expiry=*/base::Days(4),
                /*report_window=*/base::Days(4), SourceType::kEvent);
  EXPECT_EQ(source_time + base::Days(4) + base::Hours(1),
            AttributionStorageDelegateImpl().GetEventLevelReportTime(
                *report.GetStoredSource(), report.attribution_info().time));
}

TEST(AttributionStorageDelegateImplTest,
     ImpressionEventReportWindowBeforeExpiry_ReportWindowUsed) {
  base::Time source_time = base::Time::Now();
  base::Time trigger_time = source_time + base::Days(3);

  // Set the impression to expire before the two day window.
  const AttributionReport report = GetReport(source_time, trigger_time,
                                             /*expiry=*/base::Days(5),
                                             /*report_window=*/base::Days(4));
  EXPECT_EQ(source_time + base::Days(4) + base::Hours(1),
            AttributionStorageDelegateImpl().GetEventLevelReportTime(
                *report.GetStoredSource(), report.attribution_info().time));
}

TEST(AttributionStorageDelegateImplTest, GetAggregatableReportTime) {
  base::Time trigger_time = base::Time::Now();
  EXPECT_THAT(
      AttributionStorageDelegateImpl().GetAggregatableReportTime(trigger_time),
      AllOf(Ge(trigger_time + base::Minutes(10)),
            Lt(trigger_time + base::Hours(1))));
}

TEST(AttributionStorageDelegateImplTest, NewReportID_IsValidGUID) {
  EXPECT_TRUE(AttributionStorageDelegateImpl().NewReportID().is_valid());
}

TEST(AttributionStorageDelegateImplTest,
     RandomizedResponse_NoNoiseModeReturnsNull) {
  for (auto source_type : kSourceTypes) {
    const auto source =
        SourceBuilder().SetSourceType(source_type).BuildStored();
    EXPECT_EQ(AttributionStorageDelegateImpl(AttributionNoiseMode::kNone)
                  .GetRandomizedResponse(source.common_info(),
                                         source.event_report_window_time()),
              absl::nullopt);
  }
}

TEST(AttributionStorageDelegateImplTest, GetFakeReportsForSequenceIndex) {
  constexpr base::Time kImpressionTime = base::Time();
  constexpr base::TimeDelta kExpiry = base::Days(9);

  constexpr base::Time kEarlyReportTime1 =
      kImpressionTime + base::Days(2) + base::Hours(1);
  constexpr base::Time kEarlyReportTime2 =
      kImpressionTime + base::Days(7) + base::Hours(1);
  constexpr base::Time kExpiryReportTime =
      kImpressionTime + kExpiry + base::Hours(1);

  const struct {
    SourceType source_type;
    int sequence_index;
    std::vector<FakeReport> expected;
  } kTestCases[] = {
      // Event sources only have 3 output states, so we can enumerate them:
      {
          .source_type = SourceType::kEvent,
          .sequence_index = 0,
          .expected = {},
      },
      {
          .source_type = SourceType::kEvent,
          .sequence_index = 1,
          .expected = {{
              .trigger_data = 0,
              .trigger_time = kExpiryReportTime - base::Hours(1),
              .report_time = kExpiryReportTime,
          }},
      },
      {
          .source_type = SourceType::kEvent,
          .sequence_index = 2,
          .expected = {{
              .trigger_data = 1,
              .trigger_time = kExpiryReportTime - base::Hours(1),
              .report_time = kExpiryReportTime,
          }},
      },
      // Navigation sources have 2925 output states, so pick interesting ones:
      {
          .source_type = SourceType::kNavigation,
          .sequence_index = 0,
          .expected = {},
      },
      {
          .source_type = SourceType::kNavigation,
          .sequence_index = 20,
          .expected = {{
              .trigger_data = 3,
              .trigger_time = kEarlyReportTime1 - base::Hours(1),
              .report_time = kEarlyReportTime1,
          }},
      },
      {
          .source_type = SourceType::kNavigation,
          .sequence_index = 41,
          .expected =
              {
                  {
                      .trigger_data = 4,
                      .trigger_time = kEarlyReportTime1 - base::Hours(1),
                      .report_time = kEarlyReportTime1,
                  },
                  {
                      .trigger_data = 2,
                      .trigger_time = kEarlyReportTime1 - base::Hours(1),
                      .report_time = kEarlyReportTime1,
                  },
              },
      },
      {
          .source_type = SourceType::kNavigation,
          .sequence_index = 50,
          .expected =
              {
                  {
                      .trigger_data = 4,
                      .trigger_time = kEarlyReportTime1 - base::Hours(1),
                      .report_time = kEarlyReportTime1,
                  },
                  {
                      .trigger_data = 4,
                      .trigger_time = kEarlyReportTime1 - base::Hours(1),
                      .report_time = kEarlyReportTime1,
                  },
              },
      },
      {
          .source_type = SourceType::kNavigation,
          .sequence_index = 1268,
          .expected =
              {
                  {
                      .trigger_data = 1,
                      .trigger_time = kExpiryReportTime - base::Hours(1),
                      .report_time = kExpiryReportTime,
                  },
                  {
                      .trigger_data = 6,
                      .trigger_time = kEarlyReportTime2 - base::Hours(1),
                      .report_time = kEarlyReportTime2,
                  },
                  {
                      .trigger_data = 7,
                      .trigger_time = kEarlyReportTime1 - base::Hours(1),
                      .report_time = kEarlyReportTime1,
                  },
              },
      },
  };

  for (const auto& test_case : kTestCases) {
    const auto source = SourceBuilder(kImpressionTime)
                            .SetSourceType(test_case.source_type)
                            .SetExpiry(kExpiry)
                            .BuildStored();
    base::TimeDelta expiry_deadline = ExpiryDeadline(
        source.common_info().source_time(), source.event_report_window_time());
    std::vector<base::TimeDelta> deadlines =
        AttributionStorageDelegateImpl().EffectiveDeadlines(
            source.common_info().source_type(), expiry_deadline);
    EXPECT_EQ(test_case.expected,
              AttributionStorageDelegateImpl().GetFakeReportsForSequenceIndex(
                  source.common_info(), deadlines, test_case.sequence_index))
        << test_case.sequence_index;
  }
}

TEST(AttributionStorageDelegateImplTest,
     GetRandomFakeReports_Event_MatchesExpectedDistribution) {
  // The probability that not all of the 3 states are seen after `num_samples`
  // trials is at most ~1e-14476, which is 0 for all practical purposes, so the
  // `expected_num_combinations` check should always pass.
  //
  // For the distribution check, the probability of failure with `tolerance` is
  // at most 1e-9.
  RunRandomFakeReportsTest(SourceType::kEvent,
                           /*num_stars=*/1,
                           /*num_bars=*/2,
                           /*num_samples=*/100'000,
                           /*tolerance=*/0.03);
}

TEST(AttributionStorageDelegateImplTest,
     GetRandomFakeReports_Navigation_MatchesExpectedDistribution) {
  // The probability that not all of the 2925 states are seen after
  // `num_samples` trials is at most ~1e-19, which is 0 for all practical
  // purposes, so the `expected_num_combinations` check should always pass.
  //
  // For the distribution check, the probability of failure with `tolerance` is
  // at most .0002.
  RunRandomFakeReportsTest(SourceType::kNavigation,
                           /*num_stars=*/3,
                           /*num_bars=*/24,
                           /*num_samples=*/150'000,
                           /*tolerance=*/0.9);
}

TEST(AttributionStorageDelegateImplTest, SanitizeTriggerData) {
  const struct {
    SourceType source_type;
    uint64_t trigger_data;
    uint64_t expected;
  } kTestCases[] = {
      {SourceType::kNavigation, 7, 7},  //
      {SourceType::kNavigation, 8, 0},  //
      {SourceType::kNavigation, 9, 1},  //
      {SourceType::kEvent, 1, 1},       //
      {SourceType::kEvent, 2, 0},       //
      {SourceType::kEvent, 3, 1},       //
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(test_case.expected,
              AttributionStorageDelegateImpl().SanitizeTriggerData(
                  test_case.trigger_data, test_case.source_type));
  }
}

TEST(AttributionStorageDelegateImplTest, NoExpiryForImpression_DefaultUsed) {
  const base::Time source_time = base::Time::Now();

  for (auto source_type : kSourceTypes) {
    EXPECT_EQ(source_time + base::Days(30),
              AttributionStorageDelegateImpl().GetExpiryTime(
                  /*declared_expiry=*/absl::nullopt, source_time, source_type));
  }
}

TEST(AttributionStorageDelegateImplTest,
     NoReportWindowForImpression_NullOptReturned) {
  EXPECT_EQ(absl::nullopt, AttributionStorageDelegateImpl().GetReportWindowTime(
                               /*declared_window=*/absl::nullopt,
                               /*source_time=*/base::Time::Now()));
}

TEST(AttributionStorageDelegateImplTest,
     LargeImpressionExpirySpecified_ClampedTo30Days) {
  constexpr base::TimeDelta declared_expiry = base::Days(60);
  const base::Time source_time = base::Time::Now();

  for (auto source_type : kSourceTypes) {
    EXPECT_EQ(source_time + base::Days(30),
              AttributionStorageDelegateImpl().GetExpiryTime(
                  declared_expiry, source_time, source_type));
  }
}

TEST(AttributionStorageDelegateImplTest,
     LargeReportWindowSpecified_ClampedTo30Days) {
  constexpr base::TimeDelta declared_report_window = base::Days(60);
  const base::Time source_time = base::Time::Now();

  EXPECT_EQ(source_time + base::Days(30),
            AttributionStorageDelegateImpl().GetReportWindowTime(
                declared_report_window, source_time));
}

TEST(AttributionStorageDelegateImplTest,
     SmallImpressionExpirySpecified_ClampedTo1Day) {
  const struct {
    base::TimeDelta declared_expiry;
    base::TimeDelta want_expiry;
  } kTestCases[] = {
      {base::Days(-1), base::Days(1)},
      {base::Days(0), base::Days(1)},
      {base::Days(1) - base::Milliseconds(1), base::Days(1)},
  };

  const base::Time source_time = base::Time::Now();

  for (auto source_type : kSourceTypes) {
    for (const auto& test_case : kTestCases) {
      EXPECT_EQ(source_time + test_case.want_expiry,
                AttributionStorageDelegateImpl().GetExpiryTime(
                    test_case.declared_expiry, source_time, source_type));
    }
  }
}

TEST(AttributionStorageDelegateImplTest,
     SmallReportWindowSpecified_ClampedTo1Day) {
  const struct {
    base::TimeDelta declared_report_window;
    base::TimeDelta want_report_window;
  } kTestCases[] = {
      {base::Days(-1), base::Days(1)},
      {base::Days(0), base::Days(1)},
      {base::Days(1) - base::Milliseconds(1), base::Days(1)},
  };

  const base::Time source_time = base::Time::Now();

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(source_time + test_case.want_report_window,
              AttributionStorageDelegateImpl().GetReportWindowTime(
                  test_case.declared_report_window, source_time));
  }
}

TEST(AttributionStorageDelegateImplTest,
     NonWholeDayImpressionExpirySpecified_Rounded) {
  const struct {
    SourceType source_type;
    base::TimeDelta declared_expiry;
    base::TimeDelta want_expiry;
  } kTestCases[] = {
      {SourceType::kNavigation, base::Hours(36), base::Hours(36)},
      {SourceType::kEvent, base::Hours(36), base::Days(2)},

      {SourceType::kNavigation, base::Days(1) + base::Milliseconds(1),
       base::Days(1) + base::Milliseconds(1)},
      {SourceType::kEvent, base::Days(1) + base::Milliseconds(1),
       base::Days(1)},
  };

  const base::Time source_time = base::Time::Now();

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(
        source_time + test_case.want_expiry,
        AttributionStorageDelegateImpl().GetExpiryTime(
            test_case.declared_expiry, source_time, test_case.source_type));
  }
}

TEST(AttributionStorageDelegateImplTest,
     ImpressionExpirySpecified_ExpiryOverrideDefault) {
  constexpr base::TimeDelta declared_expiry = base::Days(10);
  const base::Time source_time = base::Time::Now();

  for (auto source_type : kSourceTypes) {
    EXPECT_EQ(source_time + base::Days(10),
              AttributionStorageDelegateImpl().GetExpiryTime(
                  declared_expiry, source_time, source_type));
  }
}

TEST(AttributionStorageDelegateImplTest,
     ReportWindowSpecified_WindowOverrideDefault) {
  constexpr base::TimeDelta declared_expiry =
      base::Days(10) + base::Milliseconds(1);
  const base::Time source_time = base::Time::Now();

  // Verify no rounding occurs.
  EXPECT_EQ(source_time + declared_expiry,
            AttributionStorageDelegateImpl().GetReportWindowTime(
                declared_expiry, source_time));
}

class AttributionStorageDelegateImplTestFeatureConfigured
    : public testing::Test {
 public:
  AttributionStorageDelegateImplTestFeatureConfigured() {
    feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kConversionMeasurement,
          {{"first_report_window_deadline", "1d"},
           {"second_report_window_deadline", "5d"},
           {"aggregate_report_min_delay", "1m"},
           {"aggregate_report_delay_span", "29m"}}}},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(AttributionStorageDelegateImplTestFeatureConfigured,
       ImmediateConversion_FirstFeatureWindowUsed) {
  base::Time source_time = base::Time::Now();
  const AttributionReport report =
      GetReport(source_time, /*trigger_time=*/source_time);
  EXPECT_EQ(source_time + base::Days(1) + base::Hours(1),
            AttributionStorageDelegateImpl().GetEventLevelReportTime(
                *report.GetStoredSource(), report.attribution_info().time));
}

TEST_F(AttributionStorageDelegateImplTestFeatureConfigured,
       ConversionImmediatelyAfterWindow_NextFeatureWindowUsed) {
  base::Time source_time = base::Time::Now();

  // The deadline for a window is 1 hour before the window. Use a time just
  // after the deadline.
  base::Time trigger_time = source_time + base::Days(1) + base::Minutes(1);
  const AttributionReport report = GetReport(source_time, trigger_time);
  EXPECT_EQ(source_time + base::Days(5) + base::Hours(1),
            AttributionStorageDelegateImpl().GetEventLevelReportTime(
                *report.GetStoredSource(), report.attribution_info().time));
}

TEST_F(AttributionStorageDelegateImplTestFeatureConfigured,
       GetFeatureAggregatableReportTime) {
  base::Time trigger_time = base::Time::Now();
  EXPECT_THAT(
      AttributionStorageDelegateImpl().GetAggregatableReportTime(trigger_time),
      AllOf(Ge(trigger_time + base::Minutes(1)),
            Lt(trigger_time + base::Minutes(30))));
}

// Verifies that field test params are validated correctly.
class AttributionStorageDelegateImplTestInvalidFeatureConfigured
    : public testing::Test {
 public:
  AttributionStorageDelegateImplTestInvalidFeatureConfigured() {
    feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kConversionMeasurement,
          {{"first_report_window_deadline", "-1d"},
           {"second_report_window_deadline", "-5d"},
           {"aggregate_report_min_delay", "-1m"},
           {"aggregate_report_delay_span", "-29m"}}}},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(AttributionStorageDelegateImplTestInvalidFeatureConfigured,
       NegativeFirstWindow_DefaultUsed) {
  base::Time source_time = base::Time::Now();
  const AttributionReport report =
      GetReport(source_time, /*trigger_time=*/source_time);
  EXPECT_EQ(source_time + base::Days(2) + base::Hours(1),
            AttributionStorageDelegateImpl().GetEventLevelReportTime(
                *report.GetStoredSource(), report.attribution_info().time));
}

TEST_F(AttributionStorageDelegateImplTestInvalidFeatureConfigured,
       NegativeSecondWindow_DefaultUsed) {
  base::Time source_time = base::Time::Now();
  base::Time trigger_time = source_time + base::Days(2) + base::Minutes(1);
  const AttributionReport report = GetReport(source_time, trigger_time);
  EXPECT_EQ(source_time + base::Days(7) + base::Hours(1),
            AttributionStorageDelegateImpl().GetEventLevelReportTime(
                *report.GetStoredSource(), report.attribution_info().time));
}

TEST_F(AttributionStorageDelegateImplTestInvalidFeatureConfigured,
       NegativeAggregateParams_DefaultsUsed) {
  base::Time trigger_time = base::Time::Now();
  EXPECT_THAT(
      AttributionStorageDelegateImpl().GetAggregatableReportTime(trigger_time),
      AllOf(Ge(trigger_time + base::Minutes(10)),
            Lt(trigger_time + base::Hours(1))));
}

}  // namespace
}  // namespace content
