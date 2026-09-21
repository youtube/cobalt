/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/goog_cc_scream_network_controller/goog_cc_scream_network_controller.h"

#include <utility>

#include "api/environment/environment.h"
#include "api/transport/ecn_marking.h"
#include "api/transport/network_control.h"
#include "api/transport/network_types.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "modules/congestion_controller/goog_cc/goog_cc_network_control.h"
#include "test/create_test_environment.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

GoogCcScreamNetworkController CreateController(Environment env) {
  NetworkControllerConfig config(env);
  config.constraints.at_time = env.clock().CurrentTime();
  config.constraints.starting_rate = DataRate::KilobitsPerSec(100);
  GoogCcConfig goog_cc_config;
  return GoogCcScreamNetworkController(config, std::move(goog_cc_config));
}

TEST(GoogCcScreamNetworkControllerTest, CreateWithoutFieldTrial) {
  Environment env = CreateTestEnvironment();
  GoogCcScreamNetworkController controller = CreateController(env);
  EXPECT_EQ(controller.CurrentControllerType(), "GoogCC");
  EXPECT_FALSE(controller.SupportsEcnAdaptation());
}

TEST(GoogCcScreamNetworkControllerTest, CreateWithScreamAlways) {
  Environment env = CreateTestEnvironment(
      {.field_trials = "WebRTC-Bwe-ScreamV2/mode:always/"});
  NetworkControllerConfig config(env);
  GoogCcScreamNetworkController controller = CreateController(env);
  EXPECT_EQ(controller.CurrentControllerType(), "ScreamV2");
  EXPECT_TRUE(controller.SupportsEcnAdaptation());
}

TEST(GoogCcScreamNetworkControllerTest, CreateWithScreamAfterCe) {
  Environment env = CreateTestEnvironment(
      {.field_trials = "WebRTC-Bwe-ScreamV2/mode:only_after_ce/"});
  GoogCcScreamNetworkController controller = CreateController(env);
  EXPECT_EQ(controller.CurrentControllerType(), "GoogCC");
  EXPECT_TRUE(controller.SupportsEcnAdaptation());

  // Send a feedback with ECN CE.
  TransportPacketsFeedback feedback;
  feedback.feedback_time = env.clock().CurrentTime();
  feedback.smoothed_rtt = TimeDelta::Millis(10);
  PacketResult packet_result;
  packet_result.sent_packet.send_time = env.clock().CurrentTime();
  packet_result.sent_packet.sequence_number = 1;
  packet_result.sent_packet.size = DataSize::Bytes(100);
  packet_result.receive_time = env.clock().CurrentTime();
  packet_result.ecn = EcnMarking::kCe;
  feedback.packet_feedbacks.push_back(packet_result);
  controller.OnTransportPacketsFeedback(feedback);

  EXPECT_EQ(controller.CurrentControllerType(), "ScreamV2");
  EXPECT_TRUE(controller.SupportsEcnAdaptation());
}

TEST(GoogCcScreamNetworkControllerTest, CreateWithGoogCcWithEct1) {
  Environment env = CreateTestEnvironment(
      {.field_trials = "WebRTC-Bwe-ScreamV2/mode:goog_cc_with_ect1/"});
  GoogCcScreamNetworkController controller = CreateController(env);
  EXPECT_EQ(controller.CurrentControllerType(), "GoogCC");
  EXPECT_TRUE(controller.SupportsEcnAdaptation());

  // Send a feedback with ECN CE.
  TransportPacketsFeedback feedback;
  feedback.feedback_time = env.clock().CurrentTime();
  feedback.smoothed_rtt = TimeDelta::Millis(10);
  PacketResult packet_result;
  packet_result.sent_packet.send_time = env.clock().CurrentTime();
  packet_result.sent_packet.sequence_number = 1;
  packet_result.sent_packet.size = DataSize::Bytes(100);
  packet_result.receive_time = env.clock().CurrentTime();
  packet_result.ecn = EcnMarking::kCe;
  feedback.packet_feedbacks.push_back(packet_result);
  controller.OnTransportPacketsFeedback(feedback);

  EXPECT_EQ(controller.CurrentControllerType(), "GoogCC");
  EXPECT_FALSE(controller.SupportsEcnAdaptation());
}

}  // namespace
}  // namespace webrtc
