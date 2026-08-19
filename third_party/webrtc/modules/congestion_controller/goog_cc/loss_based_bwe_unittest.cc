/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/goog_cc/loss_based_bwe.h"

#include <optional>

#include "api/field_trials.h"
#include "api/transport/network_types.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/congestion_controller/goog_cc/loss_based_bwe_v2.h"
#include "test/create_test_field_trials.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

TEST(LossBasedBweTest, ReturnFractionLoss) {
  FieldTrials field_trials = CreateTestFieldTrials();
  LossBasedBwe loss_based_bwe(&field_trials);
  loss_based_bwe.SetConfiguredMinMaxBitrate(DataRate::KilobitsPerSec(50),
                                            DataRate::KilobitsPerSec(500));
  loss_based_bwe.SetStartRate(DataRate::KilobitsPerSec(100));
  EXPECT_EQ(loss_based_bwe.GetEstimate(), DataRate::KilobitsPerSec(100));

  // 25% packet loss.
  loss_based_bwe.OnPacketLossReport(/*packets_lost=*/5,
                                    /*packets_received=*/20,
                                    /*round_trip_time=*/TimeDelta::Millis(10),
                                    Timestamp::Seconds(123));
  EXPECT_EQ(loss_based_bwe.fraction_loss(), (5 << 8) / (20 + 5));

  loss_based_bwe.OnPacketLossReport(
      /*packets_lost=*/0,
      /*packets_received=*/2,
      /*round_trip_time=*/TimeDelta::Millis(10),
      Timestamp::Seconds(123) + TimeDelta::Millis(50));
  // Not enough packets for a new update. Expect old value.
  EXPECT_EQ(loss_based_bwe.fraction_loss(), (5 << 8) / (20 + 5));

  loss_based_bwe.OnPacketLossReport(
      /*packets_lost=*/0,
      /*packets_received=*/20,
      /*round_trip_time=*/TimeDelta::Millis(10),
      Timestamp::Seconds(123) + TimeDelta::Millis(70));
  EXPECT_EQ(loss_based_bwe.fraction_loss(), 0);
}

// Test that BWE react to loss even if no transport feedback is received.
TEST(LossBasedBweTest, EstimateReactToLossReport) {
  FieldTrials field_trials = CreateTestFieldTrials();
  LossBasedBwe loss_based_bwe(&field_trials);
  loss_based_bwe.SetConfiguredMinMaxBitrate(DataRate::KilobitsPerSec(50),
                                            DataRate::KilobitsPerSec(500));
  loss_based_bwe.SetStartRate(DataRate::KilobitsPerSec(100));
  EXPECT_EQ(loss_based_bwe.GetEstimate(), DataRate::KilobitsPerSec(100));

  Timestamp now = Timestamp::Seconds(123);
  for (int i = 0; i < 3; ++i) {
    now += TimeDelta::Millis(30);
    // 25% packet loss.
    loss_based_bwe.OnPacketLossReport(
        /*packets_lost=*/5,
        /*packets_received=*/20,
        /*round_trip_time=*/TimeDelta::Millis(10), now);
    loss_based_bwe.OnPeriodicProcess(now);
  }
  EXPECT_LT(loss_based_bwe.GetEstimate(), DataRate::KilobitsPerSec(100));
  // V0 loss based estimator does not change state. Probing is still allowed?
  EXPECT_EQ(loss_based_bwe.state(), LossBasedState::kDelayBasedEstimate);

  // If there is no loss, BWE eventually increase to current delay based
  // estimate.
  while (now < Timestamp::Seconds(123) + TimeDelta::Seconds(20)) {
    now += TimeDelta::Millis(30);
    loss_based_bwe.OnPacketLossReport(
        /*packets_lost=*/0,
        /*packets_received=*/20,
        /*round_trip_time=*/TimeDelta::Millis(10), now);
    loss_based_bwe.OnPeriodicProcess(now);
  }
  EXPECT_GT(loss_based_bwe.GetEstimate(), DataRate::KilobitsPerSec(100));
  EXPECT_LE(loss_based_bwe.GetEstimate(), DataRate::KilobitsPerSec(500));
  EXPECT_EQ(loss_based_bwe.state(), LossBasedState::kDelayBasedEstimate);
}

TEST(LossBasedBweTest, IsProbeRateResetBweEvenIfLossLimitedInStartPhase) {
  FieldTrials field_trials = CreateTestFieldTrials();
  LossBasedBwe loss_based_bwe(&field_trials);
  loss_based_bwe.SetConfiguredMinMaxBitrate(DataRate::KilobitsPerSec(50),
                                            DataRate::KilobitsPerSec(500));
  loss_based_bwe.SetStartRate(DataRate::KilobitsPerSec(100));
  ASSERT_EQ(loss_based_bwe.GetEstimate(), DataRate::KilobitsPerSec(100));

  Timestamp now = Timestamp::Seconds(123);
  for (int i = 0; i < 3; ++i) {
    now += TimeDelta::Millis(30);
    // 25% packet loss.
    loss_based_bwe.OnPacketLossReport(
        /*packets_lost=*/5,
        /*packets_received=*/20,
        /*round_trip_time=*/TimeDelta::Millis(10), now);
    loss_based_bwe.OnPeriodicProcess(now);
  }
  ASSERT_LT(loss_based_bwe.GetEstimate(), DataRate::KilobitsPerSec(100));

  TransportPacketsFeedback feedback;
  feedback.feedback_time = now;
  loss_based_bwe.OnTransportPacketsFeedback(
      feedback, DataRate::KilobitsPerSec(200),
      /*acknowledged_bitrate=*/std::nullopt,
      /*is_probe_rate=*/true,
      /*in_alr=*/false);
  EXPECT_EQ(loss_based_bwe.GetEstimate(), DataRate::KilobitsPerSec(200));
}

TEST(LossBasedBweTest, DelayBasedBweDoesNotResetBweIfLossLimitedInStartPhase) {
  FieldTrials field_trials = CreateTestFieldTrials();
  LossBasedBwe loss_based_bwe(&field_trials);
  loss_based_bwe.SetConfiguredMinMaxBitrate(DataRate::KilobitsPerSec(50),
                                            DataRate::KilobitsPerSec(500));
  loss_based_bwe.SetStartRate(DataRate::KilobitsPerSec(100));
  ASSERT_EQ(loss_based_bwe.GetEstimate(), DataRate::KilobitsPerSec(100));

  Timestamp now = Timestamp::Seconds(123);
  for (int i = 0; i < 3; ++i) {
    now += TimeDelta::Millis(30);
    // 25% packet loss.
    loss_based_bwe.OnPacketLossReport(
        /*packets_lost=*/5,
        /*packets_received=*/20,
        /*round_trip_time=*/TimeDelta::Millis(10), now);
    loss_based_bwe.OnPeriodicProcess(now);
  }
  ASSERT_LT(loss_based_bwe.GetEstimate(), DataRate::KilobitsPerSec(100));

  TransportPacketsFeedback feedback;
  feedback.feedback_time = now;
  loss_based_bwe.OnTransportPacketsFeedback(
      feedback, DataRate::KilobitsPerSec(200),
      /*acknowledged_bitrate=*/std::nullopt,
      /*is_probe_rate=*/false,
      /*in_alr=*/false);
  EXPECT_LT(loss_based_bwe.GetEstimate(), DataRate::KilobitsPerSec(100));
}

}  // anonymous namespace
}  // namespace webrtc
