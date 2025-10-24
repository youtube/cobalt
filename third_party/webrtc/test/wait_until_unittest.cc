/*
 *  Copyright 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/wait_until.h"

#include <memory>

#include "api/rtc_error.h"
#include "api/test/create_time_controller.h"
#include "api/test/rtc_error_matchers.h"
#include "api/test/time_controller.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/fake_clock.h"
#include "rtc_base/thread.h"
#include "system_wrappers/include/clock.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using testing::_;
using testing::AllOf;
using testing::Eq;
using testing::Ge;
using testing::Gt;
using testing::Lt;
using testing::MatchesRegex;

TEST(WaitUntilTest, ReturnsWhenConditionIsMet) {
  AutoThread thread;

  int counter = 0;
  RTCErrorOr<int> result = WaitUntil([&] { return ++counter; }, Eq(3));
  EXPECT_THAT(result, IsRtcOkAndHolds(3));
}

TEST(WaitUntilTest, ReturnsErrorWhenTimeoutIsReached) {
  AutoThread thread;
  int counter = 0;
  RTCErrorOr<int> result =
      WaitUntil([&] { return --counter; }, Eq(1),
                {.timeout = TimeDelta::Millis(10), .result_name = "counter"});
  // Only returns the last error. Note we only are checking that the error
  // message ends with a negative number rather than a specific number to avoid
  // flakiness.
  EXPECT_THAT(
      result,
      IsRtcErrorOrWithMessage(
          _, MatchesRegex(
                 "Value of: counter\nExpected: is equal to 1\nActual: -\\d+")));
}

TEST(WaitUntilTest, ErrorContainsMatcherExplanation) {
  AutoThread thread;
  int counter = 0;
  auto matcher = AllOf(Gt(0), Lt(10));
  RTCErrorOr<int> result =
      WaitUntil([&] { return --counter; }, matcher,
                {.timeout = TimeDelta::Millis(10), .result_name = "counter"});
  // Only returns the last error. Note we only are checking that the error
  // message ends with a negative number rather than a specific number to avoid
  // flakiness.
  EXPECT_THAT(
      result,
      IsRtcErrorOrWithMessage(
          _, MatchesRegex("Value of: counter\nExpected: \\(is > 0\\) and "
                          "\\(is < 10\\)\nActual: -\\d+, which doesn't match "
                          "\\(is > 0\\)")));
}

TEST(WaitUntilTest, ReturnsWhenConditionIsMetWithSimulatedClock) {
  SimulatedClock fake_clock(Timestamp::Millis(1337));

  int counter = 0;
  RTCErrorOr<int> result =
      WaitUntil([&] { return ++counter; }, Eq(3), {.clock = &fake_clock});
  EXPECT_THAT(result, IsRtcOkAndHolds(3));
  // The fake clock should have advanced at least 2ms.
  EXPECT_THAT(fake_clock.CurrentTime(), Ge(Timestamp::Millis(1339)));
}

TEST(WaitUntilTest, ReturnsWhenConditionIsMetWithThreadProcessingFakeClock) {
  ScopedFakeClock fake_clock;

  int counter = 0;
  RTCErrorOr<int> result =
      WaitUntil([&] { return ++counter; }, Eq(3), {.clock = &fake_clock});
  EXPECT_THAT(result, IsRtcOkAndHolds(3));
  // The fake clock should have advanced at least 2ms.
  EXPECT_THAT(Timestamp::Micros(fake_clock.TimeNanos() * 1000),
              Ge(Timestamp::Millis(1339)));
}

TEST(WaitUntilTest, ReturnsWhenConditionIsMetWithFakeClock) {
  FakeClock fake_clock;

  int counter = 0;
  RTCErrorOr<int> result =
      WaitUntil([&] { return ++counter; }, Eq(3), {.clock = &fake_clock});
  EXPECT_THAT(result, IsRtcOkAndHolds(3));
  // The fake clock should have advanced at least 2ms.
  EXPECT_THAT(Timestamp::Micros(fake_clock.TimeNanos() * 1000),
              Ge(Timestamp::Millis(1339)));
}

TEST(WaitUntilTest, ReturnsWhenConditionIsMetWithSimulatedTimeController) {
  std::unique_ptr<TimeController> time_controller =
      CreateSimulatedTimeController();

  int counter = 0;
  RTCErrorOr<int> result = WaitUntil([&] { return ++counter; }, Eq(3),
                                     {.clock = time_controller.get()});
  EXPECT_THAT(result, IsRtcOkAndHolds(3));
  // The fake clock should have advanced at least 2ms.
  EXPECT_THAT(time_controller->GetClock()->CurrentTime(),
              Ge(Timestamp::Millis(1339)));
}

}  // namespace
}  // namespace webrtc
