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

#include "starboard/shared/starboard/media/media_util.h"

#include <vector>

#include "starboard/common/log.h"
#include "starboard/media.h"
#include "testing/gtest/include/gtest/gtest.h"

extern bool operator==(const SbMediaColorMetadata&,
                       const SbMediaColorMetadata&);

namespace starboard::shared::starboard::media {
namespace {

std::vector<uint8_t> ToVector(const void* data, int size) {
  if (size == 0) {
    return std::vector<uint8_t>();
  }
  SB_CHECK(data);
  const uint8_t* data_as_uint8 = static_cast<const uint8_t*>(data);
  return std::vector<uint8_t>(data_as_uint8, data_as_uint8 + size);
}

TEST(AudioStreamInfoTest, DefaultCtor) {
  AudioStreamInfo audio_stream_info;

  EXPECT_EQ(audio_stream_info.codec, kSbMediaAudioCodecNone);
  // No other members should be accessed if `codec` is `kSbMediaAudioCodecNone`,
  // however we still want to make sure that the following members are empty.
  EXPECT_TRUE(audio_stream_info.mime.empty());
  EXPECT_TRUE(audio_stream_info.audio_specific_config.empty());
}

TEST(AudioStreamInfoTest, CodecNone) {
  SbMediaAudioStreamInfo sb_media_audio_stream_info = {kSbMediaAudioCodecNone};

  AudioStreamInfo audio_stream_info;
  audio_stream_info = sb_media_audio_stream_info;
  EXPECT_EQ(audio_stream_info.codec, kSbMediaAudioCodecNone);

  audio_stream_info.ConvertTo(&sb_media_audio_stream_info);
  EXPECT_EQ(sb_media_audio_stream_info.codec, kSbMediaAudioCodecNone);
}

TEST(AudioStreamInfoTest, SbMediaAudioStreamInfo) {
  SbMediaAudioStreamInfo original = {};

  original.codec = kSbMediaAudioCodecOpus;
  original.mime = "audio/webm";
  original.number_of_channels = 2;
  original.samples_per_second = 24000;
  original.bits_per_sample = 16;
  original.audio_specific_config_size = 6;
  original.audio_specific_config = "config";

  AudioStreamInfo audio_stream_info(original);

  EXPECT_EQ(original.codec, audio_stream_info.codec);
  EXPECT_EQ(original.mime, audio_stream_info.mime);
  EXPECT_EQ(original.number_of_channels, audio_stream_info.number_of_channels);
  EXPECT_EQ(original.samples_per_second, audio_stream_info.samples_per_second);
  EXPECT_EQ(original.bits_per_sample, audio_stream_info.bits_per_sample);
  EXPECT_EQ(ToVector(original.audio_specific_config,
                     original.audio_specific_config_size),
            audio_stream_info.audio_specific_config);
}

TEST(VideoStreamInfoTest, DefaultCtor) {
  VideoStreamInfo video_stream_info;

  EXPECT_EQ(video_stream_info.codec, kSbMediaVideoCodecNone);
  // No other members should be accessed if `codec` is `kSbMediaVideoCodecNone`,
  // however we still want to make sure that the following members are empty.
  EXPECT_TRUE(video_stream_info.mime.empty());
  EXPECT_TRUE(video_stream_info.max_video_capabilities.empty());
}

TEST(VideoStreamInfoTest, CodecNone) {
  SbMediaVideoStreamInfo sb_media_video_stream_info = {kSbMediaVideoCodecNone};

  VideoStreamInfo video_stream_info;
  video_stream_info = sb_media_video_stream_info;
  EXPECT_EQ(video_stream_info.codec, kSbMediaVideoCodecNone);

  video_stream_info.ConvertTo(&sb_media_video_stream_info);
  EXPECT_EQ(sb_media_video_stream_info.codec, kSbMediaVideoCodecNone);
}

TEST(VideoStreamInfoTest, SbMediaVideoStreamInfo) {
  SbMediaVideoStreamInfo original = {};

  original.codec = kSbMediaVideoCodecAv1;
  original.mime = "video/mp4";
  original.max_video_capabilities = "width=3840";
  original.frame_width = 1080;
  original.frame_height = 1920;

  VideoStreamInfo video_stream_info(original);

  EXPECT_EQ(original.codec, video_stream_info.codec);
  EXPECT_EQ(original.mime, video_stream_info.mime);
  EXPECT_EQ(original.max_video_capabilities,
            video_stream_info.max_video_capabilities);
  EXPECT_EQ(original.frame_width, video_stream_info.frame_width);
  EXPECT_EQ(original.frame_height, video_stream_info.frame_height);
  EXPECT_EQ(original.color_metadata, video_stream_info.color_metadata);
}

<<<<<<< HEAD
TEST(VideoStreamInfoTest, CobaltExtensionEnhancedAudioMediaVideoStreamInfo) {
  CobaltExtensionEnhancedAudioMediaVideoStreamInfo original = {};

  original.codec = kSbMediaVideoCodecAv1;
  original.mime = "video/mp4";
  original.max_video_capabilities = "width=3840";
  original.frame_width = 1080;
  original.frame_height = 1920;

  VideoStreamInfo video_stream_info(original);

  EXPECT_EQ(original.codec, video_stream_info.codec);
  EXPECT_EQ(original.mime, video_stream_info.mime);
  EXPECT_EQ(original.max_video_capabilities,
            video_stream_info.max_video_capabilities);
  EXPECT_EQ(original.frame_width, video_stream_info.frame_width);
  EXPECT_EQ(original.frame_height, video_stream_info.frame_height);
  EXPECT_EQ(original.color_metadata, video_stream_info.color_metadata);
}

=======
>>>>>>> 7910c6b362f (starboard: Remove the deprecated EnhancedAudio extension (#8635))
TEST(AudioSampleInfoTest, DefaultCtor) {
  AudioSampleInfo audio_sample_info;

  EXPECT_EQ(audio_sample_info.stream_info.codec, kSbMediaAudioCodecNone);
  // No other members should be accessed if `codec` is `kSbMediaAudioCodecNone`,
  // however we still want to make sure that the following members are empty.
  EXPECT_TRUE(audio_sample_info.stream_info.mime.empty());
  EXPECT_TRUE(audio_sample_info.stream_info.audio_specific_config.empty());
}

TEST(AudioSampleInfoTest, SbMediaAudioSampleInfo) {
  SbMediaAudioSampleInfo original = {};
  SbMediaAudioStreamInfo& stream_info = original.stream_info;

  stream_info.codec = kSbMediaAudioCodecOpus;
  stream_info.mime = "audio/webm";
  stream_info.number_of_channels = 2;
  stream_info.samples_per_second = 24000;
  stream_info.bits_per_sample = 16;
  stream_info.audio_specific_config_size = 6;
  stream_info.audio_specific_config = "config";

  AudioSampleInfo audio_sample_info(original);

  EXPECT_EQ(stream_info.codec, audio_sample_info.stream_info.codec);
  EXPECT_EQ(stream_info.mime, audio_sample_info.stream_info.mime);
  EXPECT_EQ(stream_info.number_of_channels,
            audio_sample_info.stream_info.number_of_channels);
  EXPECT_EQ(stream_info.samples_per_second,
            audio_sample_info.stream_info.samples_per_second);
  EXPECT_EQ(stream_info.bits_per_sample,
            audio_sample_info.stream_info.bits_per_sample);
  EXPECT_EQ(ToVector(stream_info.audio_specific_config,
                     stream_info.audio_specific_config_size),
            audio_sample_info.stream_info.audio_specific_config);
}

TEST(VideoSampleInfoTest, DefaultCtor) {
  VideoSampleInfo video_sample_info;

  EXPECT_EQ(video_sample_info.stream_info.codec, kSbMediaVideoCodecNone);
  // No other members should be accessed if `codec` is `kSbMediaVideoCodecNone`,
  // however we still want to make sure that the following members are empty.
  EXPECT_TRUE(video_sample_info.stream_info.mime.empty());
  EXPECT_TRUE(video_sample_info.stream_info.max_video_capabilities.empty());
}

TEST(VideoSampleInfoTest, SbMediaVideoSampleInfo) {
  SbMediaVideoSampleInfo original = {};
  SbMediaVideoStreamInfo& stream_info = original.stream_info;

  original.is_key_frame = true;
  stream_info.codec = kSbMediaVideoCodecAv1;
  stream_info.mime = "video/mp4";
  stream_info.max_video_capabilities = "width=3840";
  stream_info.frame_width = 1080;
  stream_info.frame_height = 1920;

  VideoSampleInfo video_sample_info(original);

  EXPECT_EQ(original.is_key_frame, video_sample_info.is_key_frame);
  EXPECT_EQ(stream_info.codec, video_sample_info.stream_info.codec);
  EXPECT_EQ(stream_info.mime, video_sample_info.stream_info.mime);
  EXPECT_EQ(stream_info.max_video_capabilities,
            video_sample_info.stream_info.max_video_capabilities);
  EXPECT_EQ(stream_info.frame_width, video_sample_info.stream_info.frame_width);
  EXPECT_EQ(stream_info.frame_height,
            video_sample_info.stream_info.frame_height);
  EXPECT_EQ(stream_info.color_metadata,
            video_sample_info.stream_info.color_metadata);
}

<<<<<<< HEAD
TEST(VideoSampleInfoTest, CobaltExtensionEnhancedAudioMediaVideoSampleInfo) {
  CobaltExtensionEnhancedAudioMediaVideoSampleInfo original = {};
  CobaltExtensionEnhancedAudioMediaVideoStreamInfo& stream_info =
      original.stream_info;

  original.is_key_frame = true;
  stream_info.codec = kSbMediaVideoCodecAv1;
  stream_info.mime = "video/mp4";
  stream_info.max_video_capabilities = "width=3840";
  stream_info.frame_width = 1080;
  stream_info.frame_height = 1920;

  VideoSampleInfo video_sample_info(original);

  EXPECT_EQ(original.is_key_frame, video_sample_info.is_key_frame);
  EXPECT_EQ(stream_info.codec, video_sample_info.stream_info.codec);
  EXPECT_EQ(stream_info.mime, video_sample_info.stream_info.mime);
  EXPECT_EQ(stream_info.max_video_capabilities,
            video_sample_info.stream_info.max_video_capabilities);
  EXPECT_EQ(stream_info.frame_width, video_sample_info.stream_info.frame_width);
  EXPECT_EQ(stream_info.frame_height,
            video_sample_info.stream_info.frame_height);
  EXPECT_EQ(stream_info.color_metadata,
            video_sample_info.stream_info.color_metadata);
}

=======
>>>>>>> 7910c6b362f (starboard: Remove the deprecated EnhancedAudio extension (#8635))
TEST(MediaUtilTest, AudioDurationToFrames) {
  EXPECT_EQ(AudioDurationToFrames(0, 48000), 0);
  EXPECT_EQ(AudioDurationToFrames(1'000'000LL / 2, 48000), 48000 / 2);
  EXPECT_EQ(AudioDurationToFrames(1'000'000LL, 48000), 48000);
  EXPECT_EQ(AudioDurationToFrames(1'000'000LL * 2, 48000), 48000 * 2);
}

TEST(MediaUtilTest, AudioFramesToDuration) {
  EXPECT_EQ(AudioFramesToDuration(0, 48000), 0);
  EXPECT_EQ(AudioFramesToDuration(48000 / 2, 48000), 1'000'000LL / 2);
  EXPECT_EQ(AudioFramesToDuration(48000, 48000), 1'000'000LL);
  EXPECT_EQ(AudioFramesToDuration(48000 * 2, 48000), 1'000'000LL * 2);
}

}  // namespace

}  // namespace starboard::shared::starboard::media
