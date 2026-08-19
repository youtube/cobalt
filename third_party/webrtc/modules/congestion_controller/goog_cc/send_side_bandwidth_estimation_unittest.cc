/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/goog_cc/send_side_bandwidth_estimation.h"

#include <cstdint>
#include <optional>

#include "api/field_trials.h"
#include "api/rtc_event_log/rtc_event.h"
#include "api/transport/network_types.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "logging/rtc_event_log/events/rtc_event_bwe_update_loss_based.h"
#include "logging/rtc_event_log/mock/mock_rtc_event_log.h"
#include "test/create_test_field_trials.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

MATCHER(LossBasedBweUpdateWithBitrateOnly, "") {
  if (arg->GetType() != RtcEvent::Type::BweUpdateLossBased) {
    return false;
  }
  auto bwe_event = static_cast<RtcEventBweUpdateLossBased*>(arg);
  return bwe_event->bitrate_bps() > 0 && bwe_event->fraction_loss() == 0;
}

MATCHER(LossBasedBweUpdateWithBitrateAndLossFraction, "") {
  if (arg->GetType() != RtcEvent::Type::BweUpdateLossBased) {
    return false;
  }
  auto bwe_event = static_cast<RtcEventBweUpdateLossBased*>(arg);
  return bwe_event->bitrate_bps() > 0 && bwe_event->fraction_loss() > 0;
}

TEST(SendSideBweTest, InitialRembAppliesImmediately) {
  ::testing::NiceMock<MockRtcEventLog> event_log;
  FieldTrials key_value_config = CreateTestFieldTrials();
  SendSideBandwidthEstimation bwe(&key_value_config, &event_log);
  int64_t now_ms = 0;
  bwe.SetBitrates(DataRate::BitsPerSec(200000), DataRate::BitsPerSec(100000),
                  DataRate::BitsPerSec(1500000), Timestamp::Millis(now_ms));

  const int kRembBps = 1000000;
  const int kSecondRembBps = kRembBps + 500000;

  bwe.UpdatePacketsLost(/*packets_lost=*/0, /*packets_received=*/1,
                        Timestamp::Millis(now_ms));
  bwe.UpdateRtt(TimeDelta::Millis(50), Timestamp::Millis(now_ms));

  // Initial REMB applies immediately.
  bwe.UpdateReceiverEstimate(Timestamp::Millis(now_ms),
                             DataRate::BitsPerSec(kRembBps));
  bwe.OnPeriodicUpdate(Timestamp::Millis(now_ms));
  EXPECT_EQ(bwe.target_rate().bps(), kRembBps);

  // Second REMB, after startphase doesn't apply immediately.
  now_ms += 2001;
  bwe.UpdateReceiverEstimate(Timestamp::Millis(now_ms),
                             DataRate::BitsPerSec(kSecondRembBps));
  bwe.OnPeriodicUpdate(Timestamp::Millis(now_ms));

  EXPECT_EQ(bwe.target_rate().bps(), kRembBps);
}

TEST(SendSideBweTest, TargetFollowProbeRateIfNoLoss) {
  ::testing::NiceMock<MockRtcEventLog> event_log;
  FieldTrials key_value_config = CreateTestFieldTrials();
  SendSideBandwidthEstimation bwe(&key_value_config, &event_log);
  constexpr Timestamp kStartTime = Timestamp::Seconds(123);
  constexpr DataRate kInitialBwe = DataRate::KilobitsPerSec(200);
  bwe.SetBitrates(kInitialBwe, DataRate::KilobitsPerSec(100),
                  DataRate::KilobitsPerSec(15000), kStartTime);
  bwe.UpdatePacketsLost(/*packets_lost=*/0, /*packets_received=*/1, kStartTime);

  ASSERT_EQ(bwe.target_rate(), kInitialBwe);

  DataRate delay_based_estimate = kInitialBwe;

  int sequence_number = 0;
  for (Timestamp now = kStartTime; now < kStartTime + TimeDelta::Seconds(5);
       now = now + TimeDelta::Seconds(1)) {
    TransportPacketsFeedback feedback;
    feedback.feedback_time = now;
    for (int i = 0; i < 100; ++i) {
      PacketResult packet;
      packet.sent_packet.sequence_number = ++sequence_number;
      packet.sent_packet.send_time = now;
      packet.sent_packet.size = DataSize::Bytes(1000);
      packet.receive_time = now;
      feedback.packet_feedbacks.push_back(packet);
    }

    bwe.OnTransportPacketsFeedback(
        feedback, delay_based_estimate,
        /*acknowledged_rate=*/delay_based_estimate / 2,
        /*is_probe_rate=*/true,
        /*in_alr=*/false);
    EXPECT_EQ(bwe.target_rate(), delay_based_estimate);
    delay_based_estimate = 2 * delay_based_estimate;
  }
}

TEST(SendSideBweTest, DoesntReapplyBitrateDecreaseWithoutFollowingRemb) {
  MockRtcEventLog event_log;
  EXPECT_CALL(event_log,
              LogProxy(LossBasedBweUpdateWithBitrateAndLossFraction()))
      .Times(2);
  FieldTrials key_value_config = CreateTestFieldTrials();
  SendSideBandwidthEstimation bwe(&key_value_config, &event_log);
  static const int kMinBitrateBps = 100000;
  static const int kInitialBitrateBps = 1000000;
  int64_t now_ms = 1000;
  bwe.SetBitrates(DataRate::BitsPerSec(kInitialBitrateBps),
                  DataRate::BitsPerSec(kMinBitrateBps),
                  DataRate::BitsPerSec(1500000), Timestamp::Millis(now_ms));

  static const uint8_t kFractionLoss = 128;
  static const int64_t kRttMs = 50;
  now_ms += 10000;

  EXPECT_EQ(kInitialBitrateBps, bwe.target_rate().bps());
  EXPECT_EQ(0, bwe.fraction_loss());
  EXPECT_EQ(0, bwe.round_trip_time().ms());

  // Signal heavy loss to go down in bitrate.
  bwe.UpdatePacketsLost(/*packets_lost=*/50, /*packets_received=*/50,
                        Timestamp::Millis(now_ms));
  bwe.UpdateRtt(TimeDelta::Millis(kRttMs), Timestamp::Millis(now_ms));

  // Trigger an update 2 seconds later to not be rate limited.
  now_ms += 1000;
  bwe.UpdatePacketsLost(/*packets_lost=*/50, /*packets_received=*/50,
                        Timestamp::Millis(now_ms));
  bwe.OnPeriodicUpdate(Timestamp::Millis(now_ms));
  EXPECT_LT(bwe.target_rate().bps(), kInitialBitrateBps);
  // Verify that the obtained bitrate isn't hitting the min bitrate, or this
  // test doesn't make sense. If this ever happens, update the thresholds or
  // loss rates so that it doesn't hit min bitrate after one bitrate update.
  EXPECT_GT(bwe.target_rate().bps(), kMinBitrateBps);
  EXPECT_EQ(kFractionLoss, bwe.fraction_loss());
  EXPECT_EQ(kRttMs, bwe.round_trip_time().ms());

  // Triggering an update shouldn't apply further downgrade nor upgrade since
  // there's no intermediate receiver block received indicating whether this
  // is currently good or not.
  int last_bitrate_bps = bwe.target_rate().bps();
  // Trigger an update 2 seconds later to not be rate limited (but it still
  // shouldn't update).
  now_ms += 1000;
  bwe.OnPeriodicUpdate(Timestamp::Millis(now_ms));

  EXPECT_EQ(last_bitrate_bps, bwe.target_rate().bps());
  // The old loss rate should still be applied though.
  EXPECT_EQ(kFractionLoss, bwe.fraction_loss());
  EXPECT_EQ(kRttMs, bwe.round_trip_time().ms());
}

TEST(RttBasedBackoff, DefaultEnabled) {
  RttBasedBackoff rtt_backoff(CreateTestFieldTrials());
  EXPECT_TRUE(rtt_backoff.rtt_limit_.IsFinite());
}

TEST(RttBasedBackoff, CanBeDisabled) {
  FieldTrials key_value_config =
      CreateTestFieldTrials("WebRTC-Bwe-MaxRttLimit/Disabled/");
  RttBasedBackoff rtt_backoff(key_value_config);
  EXPECT_TRUE(rtt_backoff.rtt_limit_.IsPlusInfinity());
}

TEST(SendSideBweTest, FractionLossIsNotOverflowed) {
  MockRtcEventLog event_log;
  FieldTrials key_value_config = CreateTestFieldTrials();
  SendSideBandwidthEstimation bwe(&key_value_config, &event_log);
  static const int kMinBitrateBps = 100000;
  static const int kInitialBitrateBps = 1000000;
  int64_t now_ms = 1000;
  bwe.SetBitrates(DataRate::BitsPerSec(kInitialBitrateBps),
                  DataRate::BitsPerSec(kMinBitrateBps),
                  DataRate::BitsPerSec(1500000), Timestamp::Millis(now_ms));

  now_ms += 10000;

  EXPECT_EQ(kInitialBitrateBps, bwe.target_rate().bps());
  EXPECT_EQ(0, bwe.fraction_loss());

  // Signal negative loss.
  bwe.UpdatePacketsLost(/*packets_lost=*/-1, /*number_of_packets=*/100,
                        Timestamp::Millis(now_ms));
  EXPECT_EQ(0, bwe.fraction_loss());
}

TEST(SendSideBweTest, RttIsAboveLimitIfRttGreaterThanLimit) {
  ::testing::NiceMock<MockRtcEventLog> event_log;
  FieldTrials key_value_config = CreateTestFieldTrials();
  SendSideBandwidthEstimation bwe(&key_value_config, &event_log);
  static const int kMinBitrateBps = 10000;
  static const int kMaxBitrateBps = 10000000;
  static const int kInitialBitrateBps = 300000;
  int64_t now_ms = 0;
  bwe.SetBitrates(DataRate::BitsPerSec(kInitialBitrateBps),
                  DataRate::BitsPerSec(kMinBitrateBps),
                  DataRate::BitsPerSec(kMaxBitrateBps),
                  Timestamp::Millis(now_ms));
  bwe.UpdatePropagationRtt(/*at_time=*/Timestamp::Millis(now_ms),
                           /*propagation_rtt=*/TimeDelta::Millis(5000));
  EXPECT_TRUE(bwe.IsRttAboveLimit());
}

TEST(SendSideBweTest, RttIsBelowLimitIfRttLessThanLimit) {
  ::testing::NiceMock<MockRtcEventLog> event_log;
  FieldTrials key_value_config = CreateTestFieldTrials();
  SendSideBandwidthEstimation bwe(&key_value_config, &event_log);
  static const int kMinBitrateBps = 10000;
  static const int kMaxBitrateBps = 10000000;
  static const int kInitialBitrateBps = 300000;
  int64_t now_ms = 0;
  bwe.SetBitrates(DataRate::BitsPerSec(kInitialBitrateBps),
                  DataRate::BitsPerSec(kMinBitrateBps),
                  DataRate::BitsPerSec(kMaxBitrateBps),
                  Timestamp::Millis(now_ms));
  bwe.UpdatePropagationRtt(/*at_time=*/Timestamp::Millis(now_ms),
                           /*propagation_rtt=*/TimeDelta::Millis(1000));
  EXPECT_FALSE(bwe.IsRttAboveLimit());
}

}  // namespace webrtc
