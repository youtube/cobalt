// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/source_registration.h"

#include <utility>

#include "base/functional/function_ref.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/aggregation_keys.h"
#include "components/attribution_reporting/destination_set.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"
#include "components/attribution_reporting/test_utils.h"
#include "net/base/schemeful_site.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace attribution_reporting {
namespace {

using ::attribution_reporting::mojom::SourceRegistrationError;

SourceRegistration SourceRegistrationWith(
    DestinationSet destination_set,
    base::FunctionRef<void(SourceRegistration&)> f) {
  SourceRegistration r(std::move(destination_set));
  f(r);
  return r;
}

TEST(SourceRegistrationTest, Parse) {
  const DestinationSet destination = *DestinationSet::Create(
      {net::SchemefulSite::Deserialize("https://d.example")});

  const struct {
    const char* desc;
    const char* json;
    base::expected<SourceRegistration, SourceRegistrationError> expected;
  } kTestCases[] = {
      {
          "invalid_json",
          "!",
          base::unexpected(SourceRegistrationError::kInvalidJson),
      },
      {
          "root_wrong_type",
          "3",
          base::unexpected(SourceRegistrationError::kRootWrongType),
      },
      {
          "required_fields_only",
          R"json({"destination":"https://d.example"})json",
          SourceRegistration(destination),
      },
      {
          "source_event_id_valid",
          R"json({"source_event_id":"1","destination":"https://d.example"})json",
          SourceRegistrationWith(
              destination,
              [](SourceRegistration& r) { r.source_event_id = 1; }),
      },
      {
          "source_event_id_wrong_type",
          R"json({"source_event_id":1,"destination":"https://d.example"})json",
          SourceRegistration(destination),
      },
      {
          "source_event_id_invalid_defaults_to_0",
          R"json({"source_event_id":"-1","destination":"https://d.example"})json",
          SourceRegistration(destination),
      },
      {
          "destination_missing",
          R"json({})json",
          base::unexpected(SourceRegistrationError::kDestinationMissing),
      },
      {
          "priority_valid",
          R"json({"priority":"-5","destination":"https://d.example"})json",
          SourceRegistrationWith(
              destination, [](SourceRegistration& r) { r.priority = -5; }),
      },
      {
          "priority_wrong_type_defaults_to_0",
          R"json({"priority":-5,"destination":"https://d.example"})json",
          SourceRegistration(destination),
      },
      {
          "priority_invalid_defaults_to_0",
          R"json({"priority":"abc","destination":"https://d.example"})json",
          SourceRegistration(destination),
      },
      {
          "expiry_valid",
          R"json({"expiry":"172801","destination":"https://d.example"})json",
          SourceRegistrationWith(
              destination,
              [](SourceRegistration& r) { r.expiry = base::Seconds(172801); }),
      },
      {
          "expiry_wrong_type",
          R"json({"expiry":172800,"destination":"https://d.example"})json",
          SourceRegistration(destination),
      },
      {
          "expiry_invalid",
          R"json({"expiry":"abc","destination":"https://d.example"})json",
          SourceRegistration(destination),
      },
      {
          "event_report_window_valid",
          R"json({"expiry":"172801","event_report_window":"86401",
          "destination":"https://d.example"})json",
          SourceRegistrationWith(destination,
                                 [](SourceRegistration& r) {
                                   r.expiry = base::Seconds(172801);
                                   r.event_report_window = base::Seconds(86401);
                                 }),
      },
      {
          "event_report_window_wrong_type",
          R"json({"expiry":"172801","event_report_window":86401,
          "destination":"https://d.example"})json",
          SourceRegistrationWith(
              destination,
              [](SourceRegistration& r) { r.expiry = base::Seconds(172801); }),
      },
      {
          "event_report_window_invalid",
          R"json({"expiry":"172801","event_report_window":"abc",
          "destination":"https://d.example"})json",
          SourceRegistrationWith(
              destination,
              [](SourceRegistration& r) { r.expiry = base::Seconds(172801); }),
      },
      {
          "aggregatable_report_window_valid",
          R"json({"expiry":"172801","aggregatable_report_window":"86401",
          "destination":"https://d.example"})json",
          SourceRegistrationWith(destination,
                                 [](SourceRegistration& r) {
                                   r.expiry = base::Seconds(172801);
                                   r.aggregatable_report_window =
                                       base::Seconds(86401);
                                 }),
      },
      {
          "aggregatable_report_window_wrong_type",
          R"json({"expiry":"172801","aggregatable_report_window":86401,
          "destination":"https://d.example"})json",
          SourceRegistrationWith(
              destination,
              [](SourceRegistration& r) { r.expiry = base::Seconds(172801); }),
      },
      {
          "aggregatable_report_window_invalid",
          R"json({"expiry":"172801","aggregatable_report_window":"abc",
          "destination":"https://d.example"})json",
          SourceRegistrationWith(
              destination,
              [](SourceRegistration& r) { r.expiry = base::Seconds(172801); }),
      },
      {
          "debug_key_valid",
          R"json({"debug_key":"5","destination":"https://d.example"})json",
          SourceRegistrationWith(
              destination, [](SourceRegistration& r) { r.debug_key = 5; }),
      },
      {
          "debug_key_invalid",
          R"json({"debug_key":"-5","destination":"https://d.example"})json",
          SourceRegistration(destination),
      },
      {
          "debug_key_wrong_type",
          R"json({"debug_key":5,"destination":"https://d.example"})json",
          SourceRegistration(destination),
      },
      {
          "filter_data_valid",
          R"json({"filter_data":{"a":["b"]},"destination":"https://d.example"})json",
          SourceRegistrationWith(
              destination,
              [](SourceRegistration& r) {
                r.filter_data = *FilterData::Create({{"a", {"b"}}});
              }),
      },
      {
          "filter_data_wrong_type",
          R"json({"filter_data":5,"destination":"https://d.example"})json",
          base::unexpected(SourceRegistrationError::kFilterDataWrongType),
      },
      {
          "aggregation_keys_valid",
          R"json({"aggregation_keys":{"a":"0x1"},"destination":"https://d.example"})json",
          SourceRegistrationWith(destination,
                                 [](SourceRegistration& r) {
                                   r.aggregation_keys =
                                       *AggregationKeys::FromKeys(
                                           {{"a", absl::MakeUint128(0, 1)}});
                                 }),
      },
      {
          "aggregation_keys_wrong_type",
          R"json({"aggregation_keys":5,"destination":"https://d.example"})json",
          base::unexpected(SourceRegistrationError::kAggregationKeysWrongType),
      },
      {
          "debug_reporting_valid",
          R"json({"debug_reporting":true,"destination":"https://d.example"})json",
          SourceRegistrationWith(
              destination,
              [](SourceRegistration& r) { r.debug_reporting = true; }),
      },
      {
          "debug_reporting_wrong_type",
          R"json({"debug_reporting":"true","destination":"https://d.example"})json",
          SourceRegistration(destination),
      },
  };

  static constexpr char kSourceRegistrationErrorMetric[] =
      "Conversions.SourceRegistrationError2";

  for (const auto& test_case : kTestCases) {
    base::HistogramTester histograms;

    auto source = SourceRegistration::Parse(test_case.json);
    EXPECT_EQ(test_case.expected, source) << test_case.desc;

    if (source.has_value()) {
      histograms.ExpectTotalCount(kSourceRegistrationErrorMetric, 0);
    } else {
      histograms.ExpectUniqueSample(kSourceRegistrationErrorMetric,
                                    source.error(), 1);
    }
  }
}

TEST(SourceRegistrationTest, ToJson) {
  const DestinationSet destination = *DestinationSet::Create(
      {net::SchemefulSite::Deserialize("https://d.example")});

  const struct {
    SourceRegistration input;
    const char* expected_json;
  } kTestCases[] = {
      {
          SourceRegistration(destination),
          R"json({
            "debug_reporting": false,
            "destination":"https://d.example",
            "priority": "0",
            "source_event_id": "0"
          })json",
      },
      {
          SourceRegistrationWith(
              destination,
              [](SourceRegistration& r) {
                r.aggregatable_report_window = base::Seconds(1);
                r.aggregation_keys = *AggregationKeys::FromKeys({{"a", 2}});
                r.debug_key = 3;
                r.debug_reporting = true;
                r.event_report_window = base::Seconds(4);
                r.expiry = base::Seconds(5);
                r.filter_data = *FilterData::Create({{"b", {}}});
                r.priority = -6;
                r.source_event_id = 7;
              }),
          R"json({
            "aggregatable_report_window": "1",
            "aggregation_keys": {"a": "0x2"},
            "debug_key": "3",
            "debug_reporting": true,
            "destination":"https://d.example",
            "event_report_window": "4",
            "expiry": "5",
            "filter_data": {"b": []},
            "priority": "-6",
            "source_event_id": "7",
          })json",
      },
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_THAT(test_case.input.ToJson(),
                base::test::IsJson(test_case.expected_json));
  }
}

}  // namespace
}  // namespace attribution_reporting
