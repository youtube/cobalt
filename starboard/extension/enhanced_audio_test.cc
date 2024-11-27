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

#include "starboard/extension/enhanced_audio.h"

#include <cstddef>

#include "starboard/extension/configuration.h"
#include "starboard/media.h"
#include "starboard/player.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace extension {

TEST(EnhancedAudioTest, VerifyBinaryLayouts) {
  // Sanity check that the layouts of the extension specific types are the same
  // as corresponding SbMedia and SbPlayer types.
  EXPECT_EQ(sizeof(CobaltExtensionEnhancedAudioMediaAudioStreamInfo),
            sizeof(SbMediaAudioStreamInfo));
  EXPECT_EQ(offsetof(CobaltExtensionEnhancedAudioMediaAudioStreamInfo, codec),
            offsetof(SbMediaAudioStreamInfo, codec));
  EXPECT_EQ(offsetof(CobaltExtensionEnhancedAudioMediaAudioStreamInfo, mime),
            offsetof(SbMediaAudioStreamInfo, mime));
  EXPECT_EQ(offsetof(CobaltExtensionEnhancedAudioMediaAudioStreamInfo,
                     number_of_channels),
            offsetof(SbMediaAudioStreamInfo, number_of_channels));
  EXPECT_EQ(offsetof(CobaltExtensionEnhancedAudioMediaAudioStreamInfo,
                     samples_per_second),
            offsetof(SbMediaAudioStreamInfo, samples_per_second));
  EXPECT_EQ(offsetof(CobaltExtensionEnhancedAudioMediaAudioStreamInfo,
                     bits_per_sample),
            offsetof(SbMediaAudioStreamInfo, bits_per_sample));
  EXPECT_EQ(offsetof(CobaltExtensionEnhancedAudioMediaAudioStreamInfo,
                     audio_specific_config_size),
            offsetof(SbMediaAudioStreamInfo, audio_specific_config_size));
  EXPECT_EQ(offsetof(CobaltExtensionEnhancedAudioMediaAudioStreamInfo,
                     audio_specific_config),
            offsetof(SbMediaAudioStreamInfo, audio_specific_config));

  EXPECT_EQ(sizeof(CobaltExtensionEnhancedAudioMediaAudioSampleInfo),
            sizeof(SbMediaAudioSampleInfo));
  EXPECT_EQ(
      offsetof(CobaltExtensionEnhancedAudioMediaAudioSampleInfo, stream_info),
      offsetof(SbMediaAudioSampleInfo, stream_info));

  EXPECT_EQ(sizeof(CobaltExtensionEnhancedAudioMediaVideoStreamInfo),
            sizeof(SbMediaVideoStreamInfo));
  EXPECT_EQ(offsetof(CobaltExtensionEnhancedAudioMediaVideoStreamInfo, codec),
            offsetof(SbMediaVideoStreamInfo, codec));
  EXPECT_EQ(offsetof(CobaltExtensionEnhancedAudioMediaVideoStreamInfo, mime),
            offsetof(SbMediaVideoStreamInfo, mime));
  EXPECT_EQ(offsetof(CobaltExtensionEnhancedAudioMediaVideoStreamInfo,
                     max_video_capabilities),
            offsetof(SbMediaVideoStreamInfo, max_video_capabilities));
  EXPECT_EQ(
      offsetof(CobaltExtensionEnhancedAudioMediaVideoStreamInfo, frame_width),
      offsetof(SbMediaVideoStreamInfo, frame_width));
  EXPECT_EQ(
      offsetof(CobaltExtensionEnhancedAudioMediaVideoStreamInfo, frame_height),
      offsetof(SbMediaVideoStreamInfo, frame_height));
  EXPECT_EQ(offsetof(CobaltExtensionEnhancedAudioMediaVideoStreamInfo,
                     color_metadata),
            offsetof(SbMediaVideoStreamInfo, color_metadata));

  EXPECT_EQ(sizeof(CobaltExtensionEnhancedAudioMediaVideoSampleInfo),
            sizeof(SbMediaVideoSampleInfo));
  EXPECT_EQ(
      offsetof(CobaltExtensionEnhancedAudioMediaVideoSampleInfo, stream_info),
      offsetof(SbMediaVideoSampleInfo, stream_info));
  EXPECT_EQ(
      offsetof(CobaltExtensionEnhancedAudioMediaVideoSampleInfo, is_key_frame),
      offsetof(SbMediaVideoSampleInfo, is_key_frame));

  EXPECT_EQ(sizeof(CobaltExtensionEnhancedAudioPlayerSampleInfo),
            sizeof(SbPlayerSampleInfo));
  EXPECT_EQ(offsetof(CobaltExtensionEnhancedAudioPlayerSampleInfo, type),
            offsetof(SbPlayerSampleInfo, type));
  EXPECT_EQ(offsetof(CobaltExtensionEnhancedAudioPlayerSampleInfo, buffer),
            offsetof(SbPlayerSampleInfo, buffer));
  EXPECT_EQ(offsetof(CobaltExtensionEnhancedAudioPlayerSampleInfo, buffer_size),
            offsetof(SbPlayerSampleInfo, buffer_size));
  EXPECT_EQ(offsetof(CobaltExtensionEnhancedAudioPlayerSampleInfo, timestamp),
            offsetof(SbPlayerSampleInfo, timestamp));
  EXPECT_EQ(offsetof(CobaltExtensionEnhancedAudioPlayerSampleInfo, side_data),
            offsetof(SbPlayerSampleInfo, side_data));
  EXPECT_EQ(
      offsetof(CobaltExtensionEnhancedAudioPlayerSampleInfo, side_data_count),
      offsetof(SbPlayerSampleInfo, side_data_count));
  EXPECT_EQ(
      offsetof(CobaltExtensionEnhancedAudioPlayerSampleInfo, audio_sample_info),
      offsetof(SbPlayerSampleInfo, audio_sample_info));
  EXPECT_EQ(
      offsetof(CobaltExtensionEnhancedAudioPlayerSampleInfo, video_sample_info),
      offsetof(SbPlayerSampleInfo, video_sample_info));
  EXPECT_EQ(offsetof(CobaltExtensionEnhancedAudioPlayerSampleInfo, drm_info),
            offsetof(SbPlayerSampleInfo, drm_info));
}

}  // namespace extension
}  // namespace starboard
