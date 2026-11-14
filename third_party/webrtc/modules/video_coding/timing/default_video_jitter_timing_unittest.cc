/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/timing/default_video_jitter_timing.h"

#include <cstdint>
#include <optional>

#include "api/field_trials.h"
#include "api/units/timestamp.h"
#include "system_wrappers/include/clock.h"
#include "test/create_test_field_trials.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

constexpr uint32_t kRtpTimestamp = 12345;
constexpr Timestamp kInitialTime = Timestamp::Millis(789);

TEST(DefaultVideoJitterTimingTest, ExtrapolatorReturnsNulloptInitially) {
  FieldTrials field_trials = CreateTestFieldTrials();
  SimulatedClock clock(kInitialTime);
  DefaultVideoJitterTiming timing(&clock, field_trials);

  EXPECT_EQ(timing.ExtrapolateLocalTime(kRtpTimestamp), std::nullopt);
}

TEST(DefaultVideoJitterTimingTest, ExtrapolatorReturnsTimeAfterUpdate) {
  FieldTrials field_trials = CreateTestFieldTrials();
  SimulatedClock clock(kInitialTime);
  DefaultVideoJitterTiming timing(&clock, field_trials);

  timing.OnCompleteTemporalUnit(kRtpTimestamp, clock.CurrentTime());
  EXPECT_EQ(timing.ExtrapolateLocalTime(kRtpTimestamp), clock.CurrentTime());
}

TEST(DefaultVideoJitterTimingTest, ExtrapolatorReturnsNulloptAfterReset) {
  FieldTrials field_trials = CreateTestFieldTrials();
  SimulatedClock clock(kInitialTime);
  DefaultVideoJitterTiming timing(&clock, field_trials);

  timing.OnCompleteTemporalUnit(kRtpTimestamp, clock.CurrentTime());
  EXPECT_EQ(timing.ExtrapolateLocalTime(kRtpTimestamp), clock.CurrentTime());

  timing.Reset();
  EXPECT_EQ(timing.ExtrapolateLocalTime(kRtpTimestamp), std::nullopt);
}

}  // namespace
}  // namespace webrtc
