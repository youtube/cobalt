// Copyright 2023 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <string>

#include "starboard/drm.h"
#include "starboard/media.h"
#include "starboard/player.h"
#include "starboard/shared/starboard/drm/drm_test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

// The Android TV `SbPlayerGetPreferredOutputMode()` implementation has special
// constraints that requires it own tests.

namespace starboard {
namespace {

using ::starboard::CreateDummyDrmSystem;

SbPlayerCreationParam GetDefaultPlaybackParam() {
  SbPlayerCreationParam creation_param = {
      kSbDrmSystemInvalid,
      {
          kSbMediaAudioCodecAac,
      },
      {
          kSbMediaVideoCodecVp9,
      },
  };

  creation_param.audio_stream_info.mime = "";
  creation_param.audio_stream_info.number_of_channels = 2;
  creation_param.audio_stream_info.samples_per_second = 48000;
  creation_param.audio_stream_info.bits_per_sample = 16;

  auto& video_stream_info = creation_param.video_stream_info;

  video_stream_info.mime = "";
  video_stream_info.max_video_capabilities = "";
  video_stream_info.frame_width = 640;
  video_stream_info.frame_height = 480;

  video_stream_info.color_metadata.bits_per_channel = 8;
  video_stream_info.color_metadata.primaries = kSbMediaPrimaryIdBt709;
  video_stream_info.color_metadata.transfer = kSbMediaTransferIdBt709;
  video_stream_info.color_metadata.matrix = kSbMediaMatrixIdBt709;

  return creation_param;
}

SbPlayerCreationParam GetSdrPlaybackParam() {
  return GetDefaultPlaybackParam();
}

SbPlayerCreationParam GetHdrPlaybackParam() {
  SbPlayerCreationParam creation_param = GetDefaultPlaybackParam();
  auto& video_stream_info = creation_param.video_stream_info;

  video_stream_info.color_metadata.bits_per_channel = 10;
  video_stream_info.color_metadata.primaries = kSbMediaPrimaryIdBt2020;
  video_stream_info.color_metadata.transfer = kSbMediaTransferId10BitBt2020;
  video_stream_info.color_metadata.matrix =
      kSbMediaMatrixIdBt2020ConstantLuminance;

  return creation_param;
}

TEST(SbPlayerGetPreferredOutputModeTest, HdrPlaybackRequiresPunchOut) {
  SbPlayerCreationParam creation_param = GetHdrPlaybackParam();

  creation_param.output_mode = kSbPlayerOutputModeDecodeToTexture;
  auto preferred_output_mode = SbPlayerGetPreferredOutputMode(&creation_param);
  EXPECT_EQ(preferred_output_mode, kSbPlayerOutputModePunchOut);
}

TEST(SbPlayerGetPreferredOutputModeTest,
     SecondaryPlayerRequiresDecodeToTexture) {
  SbPlayerCreationParam creation_param = GetSdrPlaybackParam();

  creation_param.video_stream_info.max_video_capabilities =
      "width=640; height=480";
  creation_param.output_mode = kSbPlayerOutputModePunchOut;
  auto preferred_output_mode = SbPlayerGetPreferredOutputMode(&creation_param);
  EXPECT_EQ(preferred_output_mode, kSbPlayerOutputModeDecodeToTexture);
}

TEST(SbPlayerGetPreferredOutputModeTest, SecondaryPlayerCannotBeHdr) {
  SbPlayerCreationParam creation_param = GetHdrPlaybackParam();

  creation_param.video_stream_info.max_video_capabilities =
      "width=640; height=480";
  creation_param.output_mode = kSbPlayerOutputModeDecodeToTexture;
  auto preferred_output_mode = SbPlayerGetPreferredOutputMode(&creation_param);
  EXPECT_EQ(preferred_output_mode, kSbPlayerOutputModeInvalid);
}

TEST(SbPlayerGetPreferredOutputModeTest, SecuredDrmRequiresPunchOut) {
  SbPlayerCreationParam creation_param = GetSdrPlaybackParam();

  SbDrmSystem drm_system = CreateDummyDrmSystem("com.widevine");
  ASSERT_TRUE(SbDrmSystemIsValid(drm_system));

  creation_param.drm_system = drm_system;
  creation_param.output_mode = kSbPlayerOutputModeDecodeToTexture;
  auto preferred_output_mode = SbPlayerGetPreferredOutputMode(&creation_param);
  EXPECT_EQ(preferred_output_mode, kSbPlayerOutputModePunchOut);

  SbDrmDestroySystem(drm_system);
}

TEST(SbPlayerGetPreferredOutputModeTest,
     SecuredDrmWithDecodeToTextureIsInvalid) {
  SbPlayerCreationParam creation_param = GetSdrPlaybackParam();

  SbDrmSystem drm_system = CreateDummyDrmSystem("com.widevine");
  ASSERT_TRUE(SbDrmSystemIsValid(drm_system));

  creation_param.drm_system = drm_system;
  creation_param.video_stream_info.max_video_capabilities =
      "width=640; height=480";

  creation_param.output_mode = kSbPlayerOutputModePunchOut;
  auto preferred_output_mode = SbPlayerGetPreferredOutputMode(&creation_param);
  EXPECT_EQ(preferred_output_mode, kSbPlayerOutputModeInvalid);

  creation_param.output_mode = kSbPlayerOutputModeDecodeToTexture;
  preferred_output_mode = SbPlayerGetPreferredOutputMode(&creation_param);
  EXPECT_EQ(preferred_output_mode, kSbPlayerOutputModeInvalid);

  SbDrmDestroySystem(drm_system);
}

}  // namespace
}  // namespace starboard
