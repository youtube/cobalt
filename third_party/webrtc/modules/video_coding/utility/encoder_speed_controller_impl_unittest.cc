/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <optional>

#include "api/units/time_delta.h"
#include "api/video_codecs/encoder_speed_controller.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::Eq;
using ReferenceClass = webrtc::EncoderSpeedController::ReferenceClass;

namespace webrtc {
namespace {
constexpr TimeDelta kFrameInterval = TimeDelta::Seconds(1.0 / 30.0);

EncoderSpeedController::Config GetDefaultConfig() {
  EncoderSpeedController::Config config;
  config.speed_levels = {{.speeds = {5, 5, 5, 5}},
                         {.speeds = {6, 6, 6, 6}},
                         {.speeds = {7, 7, 7, 7}}};
  config.start_speed_index = 1;
  return config;
}
}  // namespace

TEST(EncoderSpeedControllerTest, CreateFailsWithEmptySpeedLevels) {
  EncoderSpeedController::Config config;
  config.speed_levels = {};
  EXPECT_EQ(EncoderSpeedController::Create(config, kFrameInterval), nullptr);
}

TEST(EncoderSpeedControllerTest, CreateFailsWithInvalidStartSpeedIndex) {
  EncoderSpeedController::Config config;
  config.speed_levels = {{.speeds = {5, 5, 5, 5}}};
  config.start_speed_index = -1;  // Invalid index

  EXPECT_EQ(EncoderSpeedController::Create(config, kFrameInterval), nullptr);

  config.start_speed_index = 1;
  EXPECT_EQ(EncoderSpeedController::Create(config, kFrameInterval), nullptr);
}

TEST(EncoderSpeedControllerTest, CreateFailsWithInvalidFrameInterval) {
  EncoderSpeedController::Config config = GetDefaultConfig();
  EXPECT_EQ(EncoderSpeedController::Create(config, TimeDelta::Zero()), nullptr);
  EXPECT_EQ(EncoderSpeedController::Create(config, TimeDelta::Millis(-1)),
            nullptr);
  EXPECT_EQ(EncoderSpeedController::Create(config, TimeDelta::PlusInfinity()),
            nullptr);
}

TEST(EncoderSpeedControllerTest, GetEncodeSettingsBaseLayers) {
  EncoderSpeedController::Config config = GetDefaultConfig();
  config.speed_levels[0].min_qp = 25;  // Prevent dropping to speed 5 easily
  auto controller = EncoderSpeedController::Create(config, kFrameInterval);
  ASSERT_NE(controller, nullptr);

  EncoderSpeedController::FrameEncodingInfo frame_info = {
      .reference_type = ReferenceClass::kMain};

  // Starts at index 1 (speed 6)
  EXPECT_EQ(controller->GetEncodeSettings(frame_info).speed, 6);

  // Simulate high encode time to increase speed
  for (int i = 0; i < 10; ++i) {
    controller->OnEncodedFrame({.encode_time = kFrameInterval * 0.90,
                                .qp = 30,
                                .frame_info = frame_info});
  }
  // Speed should increase to 7
  EXPECT_EQ(controller->GetEncodeSettings(frame_info).speed, 7);

  // Simulate low encode time to decrease speed
  for (int i = 0; i < 20; ++i) {
    controller->OnEncodedFrame({.encode_time = kFrameInterval * 0.10,
                                .qp = 20,
                                .frame_info = frame_info});
  }
  // Speed should decrease to 6
  EXPECT_EQ(controller->GetEncodeSettings(frame_info).speed, 6);
}

TEST(EncoderSpeedControllerTest, GetEncodeSettingsKeyFrame) {
  EncoderSpeedController::Config config = GetDefaultConfig();
  auto controller = EncoderSpeedController::Create(config, kFrameInterval);
  ASSERT_NE(controller, nullptr);

  EXPECT_EQ(
      controller->GetEncodeSettings({.reference_type = ReferenceClass::kKey})
          .speed,
      6);
}

TEST(EncoderSpeedControllerTest, GetEncodeSettingsWithTemporalLayers) {
  EncoderSpeedController::Config config;
  config.speed_levels = {{.speeds = {5, 6, 7, 8}}, {.speeds = {9, 10, 11, 12}}};
  config.start_speed_index = 0;
  auto controller = EncoderSpeedController::Create(config, kFrameInterval);
  ASSERT_NE(controller, nullptr);

  EXPECT_EQ(
      controller->GetEncodeSettings({.reference_type = ReferenceClass::kKey})
          .speed,
      5);
  EXPECT_EQ(
      controller->GetEncodeSettings({.reference_type = ReferenceClass::kMain})
          .speed,
      6);
  EXPECT_EQ(
      controller
          ->GetEncodeSettings({.reference_type = ReferenceClass::kIntermediate})
          .speed,
      7);
  EXPECT_EQ(controller
                ->GetEncodeSettings(
                    {.reference_type = ReferenceClass::kNoneReference})
                .speed,
            8);
}

TEST(EncoderSpeedControllerTest, StaysAtMaxSpeed) {
  EncoderSpeedController::Config config = GetDefaultConfig();
  config.start_speed_index = 2;  // Start at max speed
  auto controller = EncoderSpeedController::Create(config, kFrameInterval);
  ASSERT_NE(controller, nullptr);

  EncoderSpeedController::FrameEncodingInfo frame_info = {
      .reference_type = ReferenceClass::kMain};

  for (int i = 0; i < 20; ++i) {
    controller->OnEncodedFrame({.encode_time = kFrameInterval * 0.95,
                                .qp = 30,
                                .frame_info = frame_info});
  }

  EXPECT_EQ(controller->GetEncodeSettings(frame_info).speed,
            7);  // Still at max speed
}

TEST(EncoderSpeedControllerTest, StaysAtMinSpeed) {
  EncoderSpeedController::Config config = GetDefaultConfig();
  config.start_speed_index = 0;  // Start at min speed
  auto controller = EncoderSpeedController::Create(config, kFrameInterval);
  ASSERT_NE(controller, nullptr);

  EncoderSpeedController::FrameEncodingInfo frame_info = {
      .reference_type = ReferenceClass::kMain};

  for (int i = 0; i < 20; ++i) {
    controller->OnEncodedFrame({.speed = 5, .frame_info = frame_info});
  }

  EXPECT_EQ(controller->GetEncodeSettings(frame_info).speed,
            5);  // Still at min speed
}

TEST(EncoderSpeedControllerTest, IncreasesSpeedOnLowQp) {
  EncoderSpeedController::Config config = GetDefaultConfig();
  config.speed_levels[1].min_qp = 20;
  config.start_speed_index = 1;
  auto controller = EncoderSpeedController::Create(config, kFrameInterval);
  ASSERT_NE(controller, nullptr);

  EncoderSpeedController::FrameEncodingInfo frame_info = {
      .reference_type = ReferenceClass::kMain};

  EXPECT_EQ(controller->GetEncodeSettings(frame_info).speed, 6);

  // Simulate low QP, normal encode time
  for (int i = 0; i < 20; ++i) {
    controller->OnEncodedFrame({.encode_time = kFrameInterval * 0.60,
                                .qp = 10,
                                .frame_info = frame_info});
  }
  // Speed should increase to 7 due to low QP
  EXPECT_EQ(controller->GetEncodeSettings(frame_info).speed, 7);
}

TEST(EncoderSpeedControllerTest, DoesNotDecreaseSpeedIfQpIsTooLow) {
  EncoderSpeedController::Config config = GetDefaultConfig();
  config.speed_levels[0].min_qp = 20;  // Min QP for speed 5 is 20
  config.start_speed_index = 1;
  auto controller = EncoderSpeedController::Create(config, kFrameInterval);
  ASSERT_NE(controller, nullptr);

  EncoderSpeedController::FrameEncodingInfo frame_info = {
      .reference_type = ReferenceClass::kMain};

  EXPECT_EQ(controller->GetEncodeSettings(frame_info).speed, 6);

  // Simulate low encode time but also low QP
  for (int i = 0; i < 20; ++i) {
    controller->OnEncodedFrame({.encode_time = kFrameInterval * 0.10,
                                .qp = 10,
                                .frame_info = frame_info});
  }
  // Speed should NOT decrease to 5 because QP is below the next level's min_qp
  EXPECT_EQ(controller->GetEncodeSettings(frame_info).speed, 6);
}
}  // namespace webrtc
