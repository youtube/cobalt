// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "media/starboard/experimental_features_unpacker.h"

#include "base/containers/flat_map.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(ExperimentalFeaturesUnpackerTest, DefaultValues) {
  base::flat_map<std::string, int64_t> empty_map;
  ExperimentalFeaturesUnpacker unpacker(empty_map);

  EXPECT_EQ(unpacker.GetMaxSamplesPerWrite(), 12);
  EXPECT_FALSE(unpacker.ForceDecodeToTexture());

  const auto& ext = unpacker.GetExtensionFeatures();
  EXPECT_FALSE(ext.allow_audio_writing_on_pause);
  EXPECT_FALSE(ext.disable_low_performance_sw_decoder);
  EXPECT_FALSE(ext.enable_av1_startup_optimization);
  EXPECT_FALSE(ext.enable_codec_output_checker);
  EXPECT_FALSE(ext.enable_video_renderer_vsp_adjustment);
  EXPECT_FALSE(ext.flush_decoder_during_reset);
  EXPECT_FALSE(ext.reset_audio_decoder);
  EXPECT_FALSE(ext.skip_flush_on_decoder_teardown);
  EXPECT_FALSE(ext.skip_video_frames_over_60_fps);
  EXPECT_EQ(ext.use_dual_threads_for_video, nullptr);
  EXPECT_EQ(ext.video_decoder_initial_preroll_count, nullptr);
  EXPECT_EQ(ext.video_renderer_min_decoded_frames, nullptr);
  EXPECT_EQ(ext.video_renderer_min_input_buffers, nullptr);
}

TEST(ExperimentalFeaturesUnpackerTest, ValidValues) {
  base::flat_map<std::string, int64_t> map = {
      {"Media.MaxSamplesPerWrite", 24},
      {"Media.ForceDecodeToTexture", 1},
      {"Media.AllowAudioWritingOnPause", 1},
      {"Media.VideoDecoderInitialPrerollCount", 50},
      {"Media.UseDualThreadsForVideo", 0},
  };
  ExperimentalFeaturesUnpacker unpacker(map);

  EXPECT_EQ(unpacker.GetMaxSamplesPerWrite(), 24);
  EXPECT_TRUE(unpacker.ForceDecodeToTexture());

  const auto& ext = unpacker.GetExtensionFeatures();
  EXPECT_TRUE(ext.allow_audio_writing_on_pause);
  EXPECT_FALSE(ext.disable_low_performance_sw_decoder);
  ASSERT_NE(ext.use_dual_threads_for_video, nullptr);
  EXPECT_FALSE(*ext.use_dual_threads_for_video);
  ASSERT_NE(ext.video_decoder_initial_preroll_count, nullptr);
  EXPECT_EQ(*ext.video_decoder_initial_preroll_count, 50);
}

TEST(ExperimentalFeaturesUnpackerTest, RangedIntValidation) {
  // Too small
  base::flat_map<std::string, int64_t> map_small = {
      {"Media.MaxSamplesPerWrite", 0},
      {"Media.VideoDecoderInitialPrerollCount", 0},
  };
  ExperimentalFeaturesUnpacker unpacker_small(map_small);
  EXPECT_EQ(unpacker_small.GetMaxSamplesPerWrite(), 12);  // Default
  EXPECT_EQ(
      unpacker_small.GetExtensionFeatures().video_decoder_initial_preroll_count,
      nullptr);

  // Too large
  base::flat_map<std::string, int64_t> map_large = {
      {"Media.MaxSamplesPerWrite", 200000},
      {"Media.VideoDecoderInitialPrerollCount", 200000},
  };
  ExperimentalFeaturesUnpacker unpacker_large(map_large);
  EXPECT_EQ(unpacker_large.GetMaxSamplesPerWrite(), 12);  // Default
  EXPECT_EQ(
      unpacker_large.GetExtensionFeatures().video_decoder_initial_preroll_count,
      nullptr);
}

}  // namespace media
