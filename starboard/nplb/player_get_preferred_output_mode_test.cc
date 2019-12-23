// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/player.h"

#include "starboard/configuration.h"
#include "testing/gtest/include/gtest/gtest.h"

#if SB_HAS(PLAYER_GET_PREFERRED_OUTPUT_MODE)

namespace starboard {
namespace nplb {
namespace {

SbMediaAudioSampleInfo GetDefaultAudioSampleInfo() {
  SbMediaAudioSampleInfo audio_sample_info = {};

  audio_sample_info.codec = kSbMediaAudioCodecAac;
  audio_sample_info.format_tag = 0xff;
  audio_sample_info.number_of_channels = 2;
  audio_sample_info.samples_per_second = 22050;
  audio_sample_info.block_alignment = 4;
  audio_sample_info.bits_per_sample = 32;
  audio_sample_info.audio_specific_config_size = 0;
  audio_sample_info.average_bytes_per_second =
      audio_sample_info.samples_per_second *
      audio_sample_info.number_of_channels * audio_sample_info.bits_per_sample /
      8;

  return audio_sample_info;
}

SbMediaVideoSampleInfo GetDefaultVideoSampleInfo() {
  SbMediaVideoSampleInfo video_sample_info = {};

  video_sample_info.codec = kSbMediaVideoCodecH264;
  video_sample_info.frame_width = 1920;
  video_sample_info.frame_height = 1080;
  video_sample_info.color_metadata.primaries = kSbMediaPrimaryIdBt709;
  video_sample_info.color_metadata.transfer = kSbMediaTransferIdBt709;
  video_sample_info.color_metadata.matrix = kSbMediaMatrixIdBt709;
  video_sample_info.color_metadata.range = kSbMediaRangeIdLimited;

  return video_sample_info;
}

TEST(SbPlayerGetPreferredOutputModeTest, SunnyDay) {
  const SbMediaAudioSampleInfo kAudioSampleInfo = GetDefaultAudioSampleInfo();
  const SbMediaVideoSampleInfo kVideoSampleInfo = GetDefaultVideoSampleInfo();
  const char* kMaxVideoCapabilities = "";

  auto output_mode = SbPlayerGetPreferredOutputMode(
      &kAudioSampleInfo, &kVideoSampleInfo, kSbDrmSystemInvalid,
      kMaxVideoCapabilities);
  ASSERT_NE(output_mode, kSbPlayerOutputModeInvalid);
  EXPECT_TRUE(SbPlayerOutputModeSupported(output_mode, kVideoSampleInfo.codec,
                                          kSbDrmSystemInvalid));
}

TEST(SbPlayerGetPreferredOutputModeTest, RainyDayInvalid) {
  const SbMediaAudioSampleInfo kAudioSampleInfo = GetDefaultAudioSampleInfo();
  const SbMediaVideoSampleInfo kVideoSampleInfo = GetDefaultVideoSampleInfo();
  const char* kMaxVideoCapabilities = "";
  const SbMediaAudioSampleInfo* kInvalidAudioSampleInfo = nullptr;
  const SbMediaVideoSampleInfo* kInvalidVideoSampleInfo = nullptr;
  const char* kInvalidVideoCapabilities = nullptr;

  auto output_mode = SbPlayerGetPreferredOutputMode(
      kInvalidAudioSampleInfo, kInvalidVideoSampleInfo, kSbDrmSystemInvalid,
      kMaxVideoCapabilities);
  EXPECT_EQ(output_mode, kSbPlayerOutputModeInvalid);

  output_mode = SbPlayerGetPreferredOutputMode(
      &kAudioSampleInfo, kInvalidVideoSampleInfo, kSbDrmSystemInvalid,
      kMaxVideoCapabilities);
  EXPECT_EQ(output_mode, kSbPlayerOutputModeInvalid);

  output_mode = SbPlayerGetPreferredOutputMode(
      kInvalidAudioSampleInfo, &kVideoSampleInfo, kSbDrmSystemInvalid,
      kMaxVideoCapabilities);
  EXPECT_EQ(output_mode, kSbPlayerOutputModeInvalid);

  output_mode = SbPlayerGetPreferredOutputMode(
      &kAudioSampleInfo, &kVideoSampleInfo, kSbDrmSystemInvalid,
      kInvalidVideoCapabilities);
  EXPECT_EQ(output_mode, kSbPlayerOutputModeInvalid);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

#endif  // SB_HAS(PLAYER_GET_PREFERRED_OUTPUT_MODE)
