/*
 *  Copyright 2025 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/scream/scream_network_controller.h"

#include "api/environment/environment.h"
#include "api/transport/network_control.h"
#include "api/transport/network_types.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/congestion_controller/scream/test/cc_feedback_generator.h"
#include "system_wrappers/include/clock.h"
#include "test/create_test_environment.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

TEST(ScreamControllerTest, CanConstruct) {
  SimulatedClock clock(Timestamp::Seconds(1'234));
  Environment env = CreateTestEnvironment({.time = &clock});
  NetworkControllerConfig config(env);
  ScreamNetworkController scream_controller(config);
}

TEST(ScreamControllerTest, OnNetworkAvailabilityUpdatesTargetRateAndPacerRate) {
  SimulatedClock clock(Timestamp::Seconds(1'234));
  Environment env = CreateTestEnvironment({.time = &clock});
  NetworkControllerConfig config(env);
  config.constraints.starting_rate = DataRate::KilobitsPerSec(123);
  ScreamNetworkController scream_controller(config);

  NetworkControlUpdate update =
      scream_controller.OnNetworkAvailability({.network_available = true});
  ASSERT_TRUE(update.has_updates());
  ASSERT_TRUE(update.target_rate.has_value());
  EXPECT_EQ(update.target_rate->target_rate, config.constraints.starting_rate);
  ASSERT_TRUE(update.pacer_config);
  EXPECT_EQ(update.pacer_config->data_window,
            *config.constraints.starting_rate * 1.5 * TimeDelta::Seconds(1));
}

TEST(ScreamControllerTest,
     OnTransportPacketsFeedbackUpdatesTargetRateAndPacerRate) {
  SimulatedClock clock(Timestamp::Seconds(1'234));
  Environment env = CreateTestEnvironment({.time = &clock});
  NetworkControllerConfig config(env);
  ScreamNetworkController scream_controller(config);

  // Simulation with infinite capacity.
  CcFeedbackGenerator feedback_generator({});

  TransportPacketsFeedback feedback =
      feedback_generator.ProcessUntilNextFeedback(DataRate::KilobitsPerSec(100),
                                                  clock);
  NetworkControlUpdate update =
      scream_controller.OnTransportPacketsFeedback(feedback);
  ASSERT_TRUE(update.has_updates());
  ASSERT_TRUE(update.target_rate.has_value());
  EXPECT_GT(update.target_rate->target_rate, DataRate::KilobitsPerSec(100));
  ASSERT_TRUE(update.pacer_config);
  EXPECT_EQ(update.pacer_config->data_window,
            update.target_rate->target_rate * 1.5 * TimeDelta::Seconds(1));
}

TEST(ScreamControllerTest,
     OnNetworkRouteChangeResetsScreamAndUpdatesTargetRate) {
  SimulatedClock clock(Timestamp::Seconds(1'234));
  Environment env = CreateTestEnvironment({.time = &clock});
  NetworkControllerConfig config(env);
  config.constraints.starting_rate = DataRate::KilobitsPerSec(50);
  ScreamNetworkController scream_controller(config);

  CcFeedbackGenerator feedback_generator({});
  DataRate send_rate = DataRate::KilobitsPerSec(100);
  for (int i = 0; i < 10; ++i) {
    TransportPacketsFeedback feedback =
        feedback_generator.ProcessUntilNextFeedback(send_rate, clock);
    NetworkControlUpdate update =
        scream_controller.OnTransportPacketsFeedback(feedback);
    if (update.target_rate.has_value()) {
      send_rate = update.target_rate->target_rate;
    }
  }
  ASSERT_GT(send_rate, DataRate::KilobitsPerSec(50));

  NetworkRouteChange route_change;
  route_change.constraints.starting_rate = config.constraints.starting_rate =
      DataRate::KilobitsPerSec(123);
  route_change.at_time = clock.CurrentTime();
  NetworkControlUpdate update =
      scream_controller.OnNetworkRouteChange(route_change);
  ASSERT_TRUE(update.has_updates());
  ASSERT_TRUE(update.target_rate.has_value());
  EXPECT_EQ(update.target_rate->target_rate,
            route_change.constraints.starting_rate);
  ASSERT_TRUE(update.pacer_config);
  EXPECT_EQ(
      update.pacer_config->data_window,
      *route_change.constraints.starting_rate * 1.5 * TimeDelta::Seconds(1));
}

TEST(ScreamControllerTest, TargetRateRampsUptoTargetConstraints) {
  SimulatedClock clock(Timestamp::Seconds(1'234));
  Environment env = CreateTestEnvironment({.time = &clock});
  NetworkControllerConfig config(env);
  config.constraints.max_data_rate = DataRate::KilobitsPerSec(300);
  ScreamNetworkController scream_controller(config);

  // Simulation with infinite capacity.
  CcFeedbackGenerator feedback_generator({});

  DataRate target_rate = DataRate::KilobitsPerSec(100);
  for (int i = 0; i < 10; ++i) {
    TransportPacketsFeedback feedback =
        feedback_generator.ProcessUntilNextFeedback(target_rate, clock);
    NetworkControlUpdate update =
        scream_controller.OnTransportPacketsFeedback(feedback);
    if (update.target_rate.has_value()) {
      target_rate = update.target_rate->target_rate;
    }
  }
  EXPECT_EQ(target_rate, DataRate::KilobitsPerSec(300));

  // Reduce the constraints and expect the next target rate is bound by it.
  TargetRateConstraints constraints;
  constraints.max_data_rate = DataRate::KilobitsPerSec(200);
  scream_controller.OnTargetRateConstraints(constraints);
  TransportPacketsFeedback feedback =
      feedback_generator.ProcessUntilNextFeedback(target_rate, clock);
  NetworkControlUpdate update =
      scream_controller.OnTransportPacketsFeedback(feedback);
  ASSERT_TRUE(update.target_rate.has_value());
  EXPECT_EQ(update.target_rate->target_rate, DataRate::KilobitsPerSec(200));
}

}  // namespace
}  // namespace webrtc
