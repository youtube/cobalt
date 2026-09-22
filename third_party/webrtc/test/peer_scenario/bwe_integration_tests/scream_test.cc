/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "api/audio_options.h"
#include "api/scoped_refptr.h"
#include "api/stats/rtc_stats_report.h"
#include "api/test/network_emulation/dual_pi2_network_queue.h"
#include "api/test/network_emulation/network_config_schedule.pb.h"
#include "api/test/network_emulation/network_queue.h"
#include "api/test/network_emulation/schedulable_network_node_builder.h"
#include "api/test/network_emulation_manager.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "test/create_frame_generator_capturer.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/network/network_emulation.h"
#include "test/peer_scenario/bwe_integration_tests/stats_utilities.h"
#include "test/peer_scenario/peer_scenario.h"
#include "test/peer_scenario/peer_scenario_client.h"

namespace webrtc {
namespace {

using test::GetAvailableSendBitrate;
using test::GetFirstReportAtOrAfter;
using test::GetPacketsReceived;
using test::GetPacketsReceivedWithCe;
using test::GetPacketsReceivedWithEct1;
using test::GetPacketsSent;
using test::GetPacketsSentWithEct1;
using test::GetStatsAndProcess;
using test::PeerScenario;
using test::PeerScenarioClient;
using ::testing::Each;
using ::testing::HasSubstr;
using ::testing::TestWithParam;

MATCHER_P2(AvailableSendBitrateIsBetween, low, high, "") {
  DataRate available_bwe = GetAvailableSendBitrate(arg);
  return available_bwe > low && available_bwe < high;
}

std::vector<EmulatedNetworkNode*> CreateNetworkPath(PeerScenario& s,
                                                    bool use_dual_pi,
                                                    DataRate link_capacity,
                                                    TimeDelta one_way_delay) {
  NetworkEmulationManager::SimulatedNetworkNode::Builder network_builder =
      s.net()
          ->NodeBuilder()
          .capacity(link_capacity)
          .delay_ms(one_way_delay.ms());
  std::unique_ptr<NetworkQueueFactory> queue_factory;
  if (use_dual_pi) {
    queue_factory = std::make_unique<DualPi2NetworkQueueFactory>(
        DualPi2NetworkQueue::Config({.target_delay = TimeDelta::Millis(10)}));
    network_builder.queue_factory(*queue_factory);
  }
  return {network_builder.Build().node};
}

std::vector<EmulatedNetworkNode*> CreateNetworkPathWithPauseBetween3sAnd6s(
    PeerScenario& s) {
  network_behaviour::NetworkConfigSchedule schedule;
  auto initial_config = schedule.add_item();
  initial_config->set_link_capacity_kbps(1000);
  auto updated_capacity = schedule.add_item();
  updated_capacity->set_time_since_first_sent_packet_ms(3000);
  updated_capacity->set_link_capacity_kbps(0);
  updated_capacity = schedule.add_item();
  updated_capacity->set_time_since_first_sent_packet_ms(6000);
  updated_capacity->set_link_capacity_kbps(1000);
  SchedulableNetworkNodeBuilder schedulable_builder(*s.net(),
                                                    std::move(schedule));
  return {schedulable_builder.Build()};
}

std::vector<EmulatedNetworkNode*> CreateNetworkPath1MbitDelayIncreaseAfter3S(
    PeerScenario& s) {
  network_behaviour::NetworkConfigSchedule schedule;
  auto initial_config = schedule.add_item();
  initial_config->set_link_capacity_kbps(1000);
  initial_config->set_queue_delay_ms(10);
  auto updated_latency = schedule.add_item();
  updated_latency->set_time_since_first_sent_packet_ms(3000);
  updated_latency->set_queue_delay_ms(80);

  SchedulableNetworkNodeBuilder schedulable_builder(*s.net(),
                                                    std::move(schedule));
  return {schedulable_builder.Build()};
}

struct SendMediaTestResult {
  // Stats gathered every second during the call.
  std::vector<scoped_refptr<const RTCStatsReport>> caller_stats;
  std::vector<scoped_refptr<const RTCStatsReport>> callee_stats;
};

struct SendMediaTestParams {
  std::vector<EmulatedNetworkNode*> caller_to_callee_path;
  std::vector<EmulatedNetworkNode*> callee_to_caller_path;
  std::map</*trial*/ std::string, /*group*/ std::string> field_trials = {
      {"WebRTC-RFC8888CongestionControlFeedback", "Enabled,offer:true"},
      {"WebRTC-Bwe-ScreamV2", "Enabled"}};

  PeerScenarioClient::VideoSendTrackConfig caller_video_conf = {
      .generator = {.squares_video =
                        test::FrameGeneratorCapturerConfig::SquaresVideo{
                            .framerate = 30,
                            .width = 1280,
                            .height = 720,
                        }}};

  TimeDelta test_duration = TimeDelta::Seconds(10);
};

// Sends audio and video from a caller to a callee with symmetric
// uplink/downlink network.
SendMediaTestResult SendMediaInOneDirection(SendMediaTestParams params,
                                            PeerScenario& s) {
  PeerScenarioClient::Config config;
  for (auto [trial, group] : params.field_trials) {
    config.field_trials.Set(trial, group);
  }
  PeerScenarioClient* caller = s.CreateClient(config);
  PeerScenarioClient* callee = s.CreateClient(config);

  caller->CreateAudio("AUDIO_1", {});
  caller->CreateVideo("VIDEO_1", params.caller_video_conf);

  s.SimpleConnection(caller, callee, std::move(params.caller_to_callee_path),
                     std::move(params.callee_to_caller_path));
  SendMediaTestResult result;

  Timestamp end_time = s.net()->Now() + params.test_duration;
  while (s.net()->Now() < end_time) {
    s.ProcessMessages(TimeDelta::Seconds(1));
    result.caller_stats.push_back(GetStatsAndProcess(s, caller));
    result.callee_stats.push_back(GetStatsAndProcess(s, callee));
  }
  return result;
}

// This test is not using Scream - it is only here as a reference.
TEST(ScreamTest, CallerAdaptsToLinkCapacity600KbpsRtt100msNoEcnWithGoogCC) {
  PeerScenario s(*testing::UnitTest::GetInstance()->current_test_info());
  SendMediaTestParams params{
      .field_trials = {
          {"WebRTC-RFC8888CongestionControlFeedback", "Enabled,offer:true"}}};
  params.callee_to_caller_path =
      CreateNetworkPath(s, /*use_dual_pi= */ false,
                        DataRate::KilobitsPerSec(600), TimeDelta::Millis(50));
  params.caller_to_callee_path =
      CreateNetworkPath(s, /*use_dual_pi= */ false,
                        DataRate::KilobitsPerSec(600), TimeDelta::Millis(50));
  SendMediaTestResult result = SendMediaInOneDirection(std::move(params), s);
  EXPECT_THAT(result.caller_stats.back(),
              AvailableSendBitrateIsBetween(DataRate::KilobitsPerSec(450),
                                            DataRate::KilobitsPerSec(660)));
}

TEST(ScreamTest, CallerAdaptsToLinkCapacity600KbpsRtt100msNoEcn) {
  PeerScenario s(*testing::UnitTest::GetInstance()->current_test_info());
  SendMediaTestParams params;
  params.callee_to_caller_path =
      CreateNetworkPath(s, /*use_dual_pi= */ false,
                        DataRate::KilobitsPerSec(600), TimeDelta::Millis(50));
  params.caller_to_callee_path =
      CreateNetworkPath(s, /*use_dual_pi= */ false,
                        DataRate::KilobitsPerSec(600), TimeDelta::Millis(50));
  SendMediaTestResult result = SendMediaInOneDirection(std::move(params), s);
  EXPECT_THAT(result.caller_stats.back(),
              AvailableSendBitrateIsBetween(DataRate::KilobitsPerSec(400),
                                            DataRate::KilobitsPerSec(800)));
}

TEST(ScreamTest, CallerAdaptsToLinkCapacity600KbpsRtt20msNoEcn) {
  PeerScenario s(*testing::UnitTest::GetInstance()->current_test_info());
  SendMediaTestParams params;
  params.callee_to_caller_path =
      CreateNetworkPath(s, /*use_dual_pi= */ false,
                        DataRate::KilobitsPerSec(600), TimeDelta::Millis(10));
  params.caller_to_callee_path =
      CreateNetworkPath(s, /*use_dual_pi= */ false,
                        DataRate::KilobitsPerSec(600), TimeDelta::Millis(10));

  SendMediaTestResult result = SendMediaInOneDirection(std::move(params), s);
  // If encoder produce at a too low rate, RTT decrease and BWE increase.
  EXPECT_THAT(result.caller_stats.back(),
              AvailableSendBitrateIsBetween(DataRate::KilobitsPerSec(400),
                                            DataRate::KilobitsPerSec(800)));
}

TEST(ScreamTest, CallerAdaptsToLinkCapacity600KbpsRtt100msEcn) {
  PeerScenario s(*testing::UnitTest::GetInstance()->current_test_info());
  SendMediaTestParams params;
  params.callee_to_caller_path =
      CreateNetworkPath(s, /*use_dual_pi= */ true,
                        DataRate::KilobitsPerSec(600), TimeDelta::Millis(50));
  params.caller_to_callee_path =
      CreateNetworkPath(s, /*use_dual_pi= */ true,
                        DataRate::KilobitsPerSec(600), TimeDelta::Millis(50));

  SendMediaTestResult result = SendMediaInOneDirection(std::move(params), s);
  EXPECT_THAT(result.caller_stats.back(),
              AvailableSendBitrateIsBetween(DataRate::KilobitsPerSec(350),
                                            DataRate::KilobitsPerSec(660)));
}

TEST(ScreamTest, CallerAdaptsToLinkCapacity600KbpsRtt100msEcnAfterCe) {
  PeerScenario s(*testing::UnitTest::GetInstance()->current_test_info());
  SendMediaTestParams params;
  params.callee_to_caller_path =
      CreateNetworkPath(s, /*use_dual_pi= */ true,
                        DataRate::KilobitsPerSec(600), TimeDelta::Millis(50));
  params.caller_to_callee_path =
      CreateNetworkPath(s, /*use_dual_pi= */ true,
                        DataRate::KilobitsPerSec(600), TimeDelta::Millis(50));
  params.field_trials = {
      {"WebRTC-RFC8888CongestionControlFeedback", "Enabled,offer:true"},
      {"WebRTC-Bwe-ScreamV2", "mode:only_after_ce"}};

  SendMediaTestResult result = SendMediaInOneDirection(std::move(params), s);
  EXPECT_THAT(result.caller_stats.back(),
              AvailableSendBitrateIsBetween(DataRate::KilobitsPerSec(350),
                                            DataRate::KilobitsPerSec(660)));

  // All packets are sent as ECT1.
  EXPECT_EQ(GetPacketsSent(result.caller_stats.back()),
            GetPacketsSentWithEct1(result.caller_stats.back()));
  // Not all packets has been received yet.
  EXPECT_GE(GetPacketsSentWithEct1(result.caller_stats.back()),
            0.9 * (GetPacketsReceived(result.callee_stats.back())));
}

// Test that we can switch from Goog CC sending ECT1 to send ECT 0 and adapt.
TEST(ScreamTest,
     CallerAdaptsToLinkCapacity600KbpsRtt100msEcnWithGoogCcAfterCe) {
  PeerScenario s(*testing::UnitTest::GetInstance()->current_test_info());
  SendMediaTestParams params;
  params.callee_to_caller_path =
      CreateNetworkPath(s, /*use_dual_pi= */ true,
                        DataRate::KilobitsPerSec(600), TimeDelta::Millis(50));
  params.caller_to_callee_path =
      CreateNetworkPath(s, /*use_dual_pi= */ true,
                        DataRate::KilobitsPerSec(600), TimeDelta::Millis(50));
  params.field_trials = {
      {"WebRTC-RFC8888CongestionControlFeedback", "Enabled,offer:true"},
      {"WebRTC-Bwe-ScreamV2", "mode:goog_cc_with_ect1"}};

  SendMediaTestResult result = SendMediaInOneDirection(std::move(params), s);
  EXPECT_THAT(result.caller_stats.back(),
              AvailableSendBitrateIsBetween(DataRate::KilobitsPerSec(350),
                                            DataRate::KilobitsPerSec(660)));

  // Not all packets are sent as ECT1 since packets are supposed to be sent as
  // not ECT if CE is detected.
  EXPECT_GT(GetPacketsSent(result.caller_stats.back()),
            GetPacketsSentWithEct1(result.caller_stats.back()));
  EXPECT_GE(GetPacketsReceivedWithCe(result.callee_stats.back()), 1);
}

TEST(ScreamTest, CallerAdaptsToLinkCapacity1000KbpsRtt100msEcn) {
  PeerScenario s(*testing::UnitTest::GetInstance()->current_test_info());
  SendMediaTestParams params;
  params.callee_to_caller_path =
      CreateNetworkPath(s, /*use_dual_pi= */ true,
                        DataRate::KilobitsPerSec(1000), TimeDelta::Millis(50));
  params.caller_to_callee_path =
      CreateNetworkPath(s, /*use_dual_pi= */ true,
                        DataRate::KilobitsPerSec(1000), TimeDelta::Millis(50));

  SendMediaTestResult result = SendMediaInOneDirection(std::move(params), s);
  EXPECT_THAT(result.caller_stats.back(),
              AvailableSendBitrateIsBetween(DataRate::KilobitsPerSec(600),
                                            DataRate::KilobitsPerSec(1000)));
}

TEST(ScreamTest, CallerAdaptsToLinkCapacity2MbpsRtt50msNoEcn) {
  PeerScenario s(*testing::UnitTest::GetInstance()->current_test_info());
  SendMediaTestParams params;
  params.callee_to_caller_path =
      CreateNetworkPath(s, /*use_dual_pi= */ false,
                        DataRate::KilobitsPerSec(2000), TimeDelta::Millis(25));
  params.caller_to_callee_path =
      CreateNetworkPath(s, /*use_dual_pi= */ false,
                        DataRate::KilobitsPerSec(2000), TimeDelta::Millis(25));

  SendMediaTestResult result = SendMediaInOneDirection(std::move(params), s);
  EXPECT_THAT(result.caller_stats.back(),
              AvailableSendBitrateIsBetween(DataRate::KilobitsPerSec(1600),
                                            DataRate::KilobitsPerSec(2601)));
}

TEST(ScreamTest, CallerAdaptsToLinkCapacity2MbpsRtt50msEcn) {
  PeerScenario s(*testing::UnitTest::GetInstance()->current_test_info());
  SendMediaTestParams params;
  params.callee_to_caller_path =
      CreateNetworkPath(s, /*use_dual_pi= */ true,
                        DataRate::KilobitsPerSec(2000), TimeDelta::Millis(25));
  params.caller_to_callee_path =
      CreateNetworkPath(s, /*use_dual_pi= */ true,
                        DataRate::KilobitsPerSec(2000), TimeDelta::Millis(25));

  SendMediaTestResult result = SendMediaInOneDirection(std::move(params), s);
  EXPECT_THAT(result.caller_stats.back(),
              AvailableSendBitrateIsBetween(DataRate::KilobitsPerSec(1500),
                                            DataRate::KilobitsPerSec(2100)));
}

TEST(ScreamTest, CallerAdaptsToLinkCapacity2MbpsRtt50msNoEcnWithGoogCC) {
  PeerScenario s(*testing::UnitTest::GetInstance()->current_test_info());
  SendMediaTestParams params;
  params.callee_to_caller_path =
      CreateNetworkPath(s, /*use_dual_pi= */ false,
                        DataRate::KilobitsPerSec(2000), TimeDelta::Millis(25));
  params.caller_to_callee_path =
      CreateNetworkPath(s, /*use_dual_pi= */ false,
                        DataRate::KilobitsPerSec(2000), TimeDelta::Millis(25));
  params.field_trials = {
      {"WebRTC-RFC8888CongestionControlFeedback", "Enabled,offer:true"},
  };

  SendMediaTestResult result = SendMediaInOneDirection(std::move(params), s);
  EXPECT_THAT(result.caller_stats.back(),
              AvailableSendBitrateIsBetween(DataRate::KilobitsPerSec(1000),
                                            DataRate::KilobitsPerSec(2600)));
}

TEST(ScreamTest, CallerPauseSendingVideoIfFeedbackNotReceived) {
  PeerScenario s(*testing::UnitTest::GetInstance()->current_test_info());
  SendMediaTestParams params;
  params.callee_to_caller_path = CreateNetworkPathWithPauseBetween3sAnd6s(s);
  params.caller_to_callee_path =
      CreateNetworkPath(s, /*use_dual_pi= */ false,
                        DataRate::KilobitsPerSec(600), TimeDelta::Millis(50));

  Timestamp start_time = s.net()->Now();
  SendMediaTestResult result = SendMediaInOneDirection(std::move(params), s);

  std::optional<scoped_refptr<const RTCStatsReport>> report_after_4s =
      GetFirstReportAtOrAfter(start_time + TimeDelta::Seconds(4),
                              result.caller_stats);
  std::optional<scoped_refptr<const RTCStatsReport>> report_after_5s =
      GetFirstReportAtOrAfter(start_time + TimeDelta::Seconds(5),
                              result.caller_stats);
  ASSERT_TRUE(report_after_4s.has_value());
  ASSERT_TRUE(report_after_5s.has_value());
  ASSERT_GT(GetPacketsSent(report_after_4s.value(), "video"), 0);
  // Audio not paused.
  EXPECT_LT(GetPacketsSent(report_after_4s.value()),
            GetPacketsSent(report_after_5s.value()));
  // video paused.
  EXPECT_EQ(GetPacketsSent(report_after_4s.value(), "video"),
            GetPacketsSent(report_after_5s.value(), "video"));
  // video resumed.
  EXPECT_LT(GetPacketsSent(report_after_4s.value(), "video"),
            GetPacketsSent(result.caller_stats.back(), "video"));

  // Target rate is within reason at the end of the call.
  // TODO: bugs.webrtc.org/447037083 - There is no pushback between pacer queue
  // encoder. If pacer queue is paused for too long, the pacer will send
  // packets too fast.
  // EXPECT_GT(GetAvailableSendBitrate(result.caller_stats.back()),
  //          DataRate::KilobitsPerSec(400));
  EXPECT_LT(GetAvailableSendBitrate(result.caller_stats.back()),
            DataRate::KilobitsPerSec(800));
}

TEST(ScreamTest, CallerResetQueueDelayEstimateAfterIncreasedFixedDelay) {
  PeerScenario s(*testing::UnitTest::GetInstance()->current_test_info());
  SendMediaTestParams params{.test_duration = TimeDelta::Seconds(35)};
  params.caller_to_callee_path = CreateNetworkPath1MbitDelayIncreaseAfter3S(s);
  params.callee_to_caller_path =
      CreateNetworkPath(s, /*use_dual_pi= */ false,
                        DataRate::KilobitsPerSec(2000), TimeDelta::Millis(10));
  SendMediaTestResult result = SendMediaInOneDirection(std::move(params), s);

  // After the increased delay, BWE should drop
  EXPECT_LT(GetAvailableSendBitrate(result.caller_stats[5]),
            DataRate::KilobitsPerSec(400));

  // But have recovered by the end of the test.
  EXPECT_THAT(result.caller_stats.back(),
              AvailableSendBitrateIsBetween(DataRate::KilobitsPerSec(700),
                                            DataRate::KilobitsPerSec(1200)));
}

TEST(ScreamTest, CallerPaceScreencastSlideChange2Mbit50msRttNoEcn) {
  PeerScenario s(*testing::UnitTest::GetInstance()->current_test_info());
  SendMediaTestParams params{.test_duration = TimeDelta::Seconds(20)};
  params.caller_to_callee_path =
      CreateNetworkPath(s, /*use_dual_pi= */ false,
                        DataRate::KilobitsPerSec(2000), TimeDelta::Millis(25));
  params.caller_to_callee_path = params.callee_to_caller_path =
      CreateNetworkPath(s, /*use_dual_pi= */ false,
                        DataRate::KilobitsPerSec(2000), TimeDelta::Millis(25));
  params.caller_video_conf = {
      .generator = {.image_slides =
                        test::FrameGeneratorCapturerConfig::ImageSlides{
                            .change_interval = TimeDelta::Seconds(5)}}};

  SendMediaTestResult result = SendMediaInOneDirection(std::move(params), s);

  // TODO: bugs.webrtc.org/447037083 - Ensure BWE does not drop too low when
  // pacing out a slide change.
  // EXPECT_THAT(result.caller_stats, Each(AvailableSendBitrateIsBetween(
  //                                     DataRate::KilobitsPerSec(1700),
  //                                     DataRate::KilobitsPerSec(2200))));
}
}  // namespace
}  // namespace webrtc
