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

#include <numeric>
#include <string>

#include "starboard/common/string.h"
#include "starboard/nplb/drm_helpers.h"
#include "starboard/nplb/maximum_player_configuration_explorer.h"
#include "starboard/nplb/media_can_play_mime_and_key_system_test_helpers.h"
#include "starboard/nplb/player_test_fixture.h"
#include "starboard/nplb/thread_helpers.h"
#include "starboard/testing/fake_graphics_context_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

using shared::starboard::media::AudioStreamInfo;
using ::starboard::testing::FakeGraphicsContextProvider;
using ::testing::ValuesIn;

typedef SbPlayerTestFixture::GroupedSamples GroupedSamples;

enum DeviceResolution : int {
  kResolution480p,
  kResolution720p,
  kResolution1080p,
  kResolution2160p,
  kResolution2k,
  kResolution4k,
  kResolution8k
};

// The requirements for each video decoder are defined based on the codec,
// resolution, DRM (Digital Rights Management), and output mode.
typedef std::tuple<SbMediaVideoCodec /* video codec */,
                   DeviceResolution /* resolution */,
                   SbPlayerOutputMode /* output_mode */,
                   std::string /* key system */>
    VideoDecodersTestConfig;

// Each requirement encompasses multiple video decoders.
typedef std::vector<VideoDecodersTestConfig> MultipleVideoDecodersTestConfig;

// Transfer the description to match the individual video decoder test
// configuration, which serve as input parameters for the |SbPlayerTestFixture|.
SbPlayerTestConfig VideoDecodersTestConfigToSbSbPlayerTestConfig(
    const VideoDecodersTestConfig& video_decoders_test_config) {
  static const std::map<std::tuple<SbMediaVideoCodec, DeviceResolution>,
                        std::string>
      test_files_lookup_table = {{{kSbMediaVideoCodecH264, kResolution480p},
                                  "beneath_the_canopy_135_avc.dmp"},
                                 {{kSbMediaVideoCodecH264, kResolution720p},
                                  "beneath_the_canopy_136_avc.dmp"},
                                 {{kSbMediaVideoCodecH264, kResolution1080p},
                                  "beneath_the_canopy_137_avc.dmp"}};
  SbPlayerTestConfig player_test_config;
  std::get<0>(player_test_config) = nullptr; /* Audio */
  std::get<1>(player_test_config) =
      test_files_lookup_table
          .at({std::get<0>(video_decoders_test_config),
               std::get<1>(video_decoders_test_config)})
          .c_str();
  std::get<2>(player_test_config) = std::get<2>(video_decoders_test_config);
  std::get<3>(player_test_config) =
      std::get<3>(video_decoders_test_config).c_str();
  return player_test_config;
}

// Get the max resolution supported by test device.
DeviceResolution GetMaxVideoCodecResolution() {
  typedef std::map<DeviceResolution, std::string> ResolutionToMediaMimeMap;

  static const std::map<DeviceResolution, std::string> resolutions = {
      {kResolution1080p, "width=1920; height=1080;"},
      {kResolution2k, "width=2560; height=1440;"},
      {kResolution4k, "width=3840; height=2160;"},
      {kResolution8k, "width=7680; height=4320;"}};

  // The resolution order for the following decoders should be arranged in
  // ascending order.
  // Media mime types of H264.
  static const ResolutionToMediaMimeMap avc_resolution_to_media_mime_type = {
      {kResolution1080p, "avc1.4d402a"},
      {kResolution2k, "avc1.64002a"},
      {kResolution4k, "avc1.64002a"}};

  // Media mime types of Vp9.
  static const ResolutionToMediaMimeMap vp9_resolution_to_media_mime_type = {
      {kResolution1080p, "vp9"},
      {kResolution2k, "vp9"},
      {kResolution4k, "vp9"}};

  // Media mime types of Av1.
  static const ResolutionToMediaMimeMap av1_resolution_to_media_mime_type = {
      {kResolution1080p, "av01.0.08M.08"},
      {kResolution2k, "av01.0.12M.08"},
      {kResolution4k, "av01.0.12M.08"},
      {kResolution8k, "av01.0.16M.08"}};

  // Media mime types of each resolutions for |SbMediaVideoCodec|.
  static const std::map<SbMediaVideoCodec,
                        std::pair<std::string, const ResolutionToMediaMimeMap*>>
      video_codec_to_media_mime_type_map = {
          {kSbMediaVideoCodecH264,
           {"video/mp4", &avc_resolution_to_media_mime_type}},
          {kSbMediaVideoCodecVp9,
           {"video/webm", &vp9_resolution_to_media_mime_type}},
          {kSbMediaVideoCodecAv1,
           {"video/webm", &av1_resolution_to_media_mime_type}}};

  // Capture the maximum resolution for each decoder.
  std::map<SbMediaVideoCodec, DeviceResolution> max_support_resolutions;

  // In the Linux platform, the SW AV1 decoder does not support Level 4.1, which
  // violates the Video Decoders Requirements. To prevent test failures, we
  // assign kResolution1080p in advance instead.
  max_support_resolutions[kSbMediaVideoCodecAv1] = kResolution1080p;

  for (const auto& media_mime_type : video_codec_to_media_mime_type_map) {
    SbMediaVideoCodec video_codec = media_mime_type.first;
    const std::string& video_type = media_mime_type.second.first;
    for (const auto& codec_mime_of_resolution :
         *media_mime_type.second.second) {
      DeviceResolution resolution = codec_mime_of_resolution.first;
      std::string content_type = video_type + "; codecs=\"" +
                                 codec_mime_of_resolution.second + "\"; " +
                                 resolutions.at(resolution) + " framerate=30";
      if (SbMediaCanPlayMimeAndKeySystem(content_type.c_str(), "") !=
          kSbMediaSupportTypeProbably) {
        break;
      }
      max_support_resolutions[video_codec] = resolution;
    }
  }

  // According to Video Decoders Requirements - support up to 1920x1080 for AVC.
  EXPECT_TRUE(max_support_resolutions[kSbMediaVideoCodecH264] >=
              kResolution1080p);
  EXPECT_TRUE(max_support_resolutions.find(kSbMediaVideoCodecVp9) !=
              max_support_resolutions.end());
  EXPECT_TRUE(max_support_resolutions.find(kSbMediaVideoCodecAv1) !=
              max_support_resolutions.end());
  // If the Av1 decoder does not support 8K (level 6.0), both Av1 and Vp9 should
  // have the same maximum resolution support for SDR (Standard Dynamic Range).
  if (max_support_resolutions[kSbMediaVideoCodecAv1] != kResolution8k) {
    EXPECT_TRUE(max_support_resolutions[kSbMediaVideoCodecAv1] ==
                max_support_resolutions[kSbMediaVideoCodecVp9]);
  }
  return max_support_resolutions[kSbMediaVideoCodecAv1];
}

std::string GetMultipleVideoDecodersTestConfigName(
    ::testing::TestParamInfo<MultipleVideoDecodersTestConfig> info) {
  static const std::map<DeviceResolution, std::string> resolution_to_string = {
      {kResolution480p, "480P"},   {kResolution720p, "720P"},
      {kResolution1080p, "1080P"}, {kResolution2k, "2K"},
      {kResolution4k, "4K"},       {kResolution8k, "8K"}};

  static const std::map<SbMediaVideoCodec, std::string> codec_to_string = {
      {kSbMediaVideoCodecH264, "Avc"},
      {kSbMediaVideoCodecVp9, "Vp9"},
      {kSbMediaVideoCodecAv1, "Av1"}};

  const auto test_configs = info.param;
  std::string name;

  int index = 0;
  for (const auto& config : test_configs) {
    if (index > 0) {
      name += "_";
    }
    name += codec_to_string.at(std::get<0>(config));
    name += "_";
    name += resolution_to_string.at(std::get<1>(config));
    name += "_key_";
    name += std::get<3>(config);
    name += (std::get<2>(config) == kSbPlayerOutputModeDecodeToTexture
                 ? "_DecodeToTexture"
                 : "_Punchout");
    ++index;
  }
  std::replace(name.begin(), name.end(), '.', '_');
  std::replace(name.begin(), name.end(), '(', '_');
  std::replace(name.begin(), name.end(), ')', '_');
  return name;
}

std::vector<MultipleVideoDecodersTestConfig> GetSupportedTestConfigs2024() {
  std::vector<MultipleVideoDecodersTestConfig> configs_to_return;

  std::vector<const char*> key_systems;
  key_systems.push_back("");
  key_systems.insert(key_systems.end(), kKeySystems,
                     kKeySystems + SB_ARRAY_SIZE_INT(kKeySystems));

  // Requirements 2.3.1.1.
  DeviceResolution max_device_resolution = GetMaxVideoCodecResolution();

  // Requirements 2.3.1.1.1
  SbMediaVideoCodec primary_video_codec;
  MultipleVideoDecodersTestConfig primary_video_test_configs;
  for (auto key_system : key_systems) {
    if (max_device_resolution == kResolution8k) {
      primary_video_codec = kSbMediaVideoCodecAv1;
    } else {
      primary_video_codec = kSbMediaVideoCodecVp9;
    }
    if (IsOutputModeSupported(kSbPlayerOutputModeDecodeToTexture,
                              kSbMediaAudioCodecNone, primary_video_codec,
                              key_system)) {
      primary_video_test_configs.emplace_back(
          std::make_tuple(primary_video_codec, max_device_resolution,
                          kSbPlayerOutputModeDecodeToTexture, key_system));
    }
  }
  EXPECT_TRUE(!primary_video_test_configs.empty());

  // Requirements 2.3.1.1.2.
  SbMediaVideoCodec secondary_video_codec = kSbMediaVideoCodecH264;
  MultipleVideoDecodersTestConfig secondary_video_test_configs;
  for (auto key_system : key_systems) {
    if (IsOutputModeSupported(kSbPlayerOutputModeDecodeToTexture,
                              kSbMediaAudioCodecNone, secondary_video_codec,
                              key_system)) {
      secondary_video_test_configs.emplace_back(
          std::make_tuple(secondary_video_codec, kResolution480p,
                          kSbPlayerOutputModeDecodeToTexture, key_system));
    }
  }
  EXPECT_TRUE(!secondary_video_test_configs.empty());

  // Combine primary and secondary stream.
  for (const auto& primary_video_test_config : primary_video_test_configs) {
    for (const auto& secondary_video_test_config :
         secondary_video_test_configs) {
      MultipleVideoDecodersTestConfig multiple_video_decoders_test_config;
      multiple_video_decoders_test_config.emplace_back(
          primary_video_test_config);
      multiple_video_decoders_test_config.emplace_back(
          secondary_video_test_config);
      configs_to_return.emplace_back(multiple_video_decoders_test_config);
    }
  }

  // Requirements 2.3.1.2.
  // Use the secondary_video_test_config specified in Requirements 2.3.1.1.2, as
  // it supports the h.264 codec as well.
  for (auto& video_test_config : secondary_video_test_configs) {
    std::get<1>(video_test_config) = kResolution720p;
    std::get<2>(video_test_config) = kSbPlayerOutputModePunchOut;

    MultipleVideoDecodersTestConfig multiple_video_decoders_test_config;
    multiple_video_decoders_test_config.emplace_back(video_test_config);
    multiple_video_decoders_test_config.emplace_back(video_test_config);
    configs_to_return.emplace_back(multiple_video_decoders_test_config);
  }

  EXPECT_TRUE(!configs_to_return.empty());
  return configs_to_return;
}

std::vector<MultipleVideoDecodersTestConfig> GetSupportedTestConfigs2025() {
  const int kMaxPlayerInstancesPerConfig = 4;
  const int kMaxTotalPlayerInstances = 4;
  FakeGraphicsContextProvider fake_graphics_context_provider;

  std::vector<MultipleVideoDecodersTestConfig> configs_to_return;

  DeviceResolution max_device_resolution = GetMaxVideoCodecResolution();

  std::vector<const char*> key_systems;
  key_systems.push_back("");
  key_systems.insert(key_systems.end(), kKeySystems,
                     kKeySystems + SB_ARRAY_SIZE_INT(kKeySystems));

  // Scenario 1
  SbMediaVideoCodec primary_video_codec;
  MultipleVideoDecodersTestConfig primary_video_test_configs;
  for (auto key_system : key_systems) {
    if (max_device_resolution == kResolution8k) {
      primary_video_codec = kSbMediaVideoCodecAv1;
    } else {
      primary_video_codec = kSbMediaVideoCodecVp9;
    }
    if (IsOutputModeSupported(kSbPlayerOutputModeDecodeToTexture,
                              kSbMediaAudioCodecNone, primary_video_codec,
                              key_system)) {
      primary_video_test_configs.emplace_back(
          std::make_tuple(primary_video_codec, max_device_resolution,
                          kSbPlayerOutputModeDecodeToTexture, key_system));
    }
  }
  EXPECT_TRUE(!primary_video_test_configs.empty());

  SbMediaVideoCodec secondary_video_codec = kSbMediaVideoCodecH264;
  DeviceResolution secondary_video_resolutuon;
  MultipleVideoDecodersTestConfig secondary_video_test_configs;
  if (max_device_resolution <= kResolution1080p) {
    secondary_video_resolutuon = kResolution480p;
  } else if (max_device_resolution <= kResolution4k) {
    secondary_video_resolutuon = kResolution720p;
  } else {
    SB_DCHECK(max_device_resolution == kResolution8k);
    secondary_video_resolutuon = kResolution1080p;
  }
  for (auto key_system : key_systems) {
    if (IsOutputModeSupported(kSbPlayerOutputModeDecodeToTexture,
                              kSbMediaAudioCodecNone, secondary_video_codec,
                              key_system)) {
      secondary_video_test_configs.emplace_back(
          std::make_tuple(secondary_video_codec, secondary_video_resolutuon,
                          kSbPlayerOutputModeDecodeToTexture, key_system));
    }
  }
  EXPECT_TRUE(!secondary_video_test_configs.empty());

  // Combine primary and secondary stream.
  for (const auto& primary_video_test_config : primary_video_test_configs) {
    for (const auto& secondary_video_test_config :
         secondary_video_test_configs) {
      MultipleVideoDecodersTestConfig multiple_video_decoders_test_config;
      multiple_video_decoders_test_config.emplace_back(
          primary_video_test_config);
      multiple_video_decoders_test_config.emplace_back(
          secondary_video_test_config);
      configs_to_return.emplace_back(multiple_video_decoders_test_config);
    }
  }

  // Scenario 2 and 3.
  secondary_video_test_configs.clear();
  if (max_device_resolution <= kResolution1080p) {
    secondary_video_resolutuon = kResolution480p;
  } else if (max_device_resolution <= kResolution4k) {
    secondary_video_resolutuon = kResolution1080p;
  } else {
    secondary_video_resolutuon = kResolution2160p;
  }
  for (auto key_system : key_systems) {
    if (IsOutputModeSupported(kSbPlayerOutputModePunchOut,
                              kSbMediaAudioCodecNone, secondary_video_codec,
                              key_system)) {
      secondary_video_test_configs.emplace_back(
          std::make_tuple(secondary_video_codec, secondary_video_resolutuon,
                          kSbPlayerOutputModePunchOut, key_system));
    }
  }

  auto reource_limit_function = [](const std::vector<int>& v,
                                   int limitation) -> bool {
    return std::accumulate(v.begin(), v.end(), 0) <= limitation;
  };
  auto total_instance_limit_function = std::bind(
      reource_limit_function, std::placeholders::_1, kMaxTotalPlayerInstances);

  MaximumPlayerConfigurationExplorer::PlayerConfigSet player_configs;
  player_configs.insert(std::make_tuple(
      kSbMediaVideoCodecH264, AudioStreamInfo(), kSbPlayerOutputModePunchOut));
  player_configs.insert(std::make_tuple(
      kSbMediaVideoCodecVp9, AudioStreamInfo(), kSbPlayerOutputModePunchOut));
  player_configs.insert(std::make_tuple(
      kSbMediaVideoCodecAv1, AudioStreamInfo(), kSbPlayerOutputModePunchOut));

  MaximumPlayerConfigurationExplorer explorer(
      player_configs, kMaxPlayerInstancesPerConfig,
      &fake_graphics_context_provider, total_instance_limit_function);
  auto max_config_set = explorer.CalculateMaxTestConfigSet();
  // There should be C(4 + 3 - 1 , 4) = 15 combinations.
  // 3: avc + av1 + vp9, 4 codec in total.
  // The requirement is to support all mix of codecs (AV1 / H.264 / VP9).
  EXPECT_TRUE(max_config_set.size() == 15);

  for (const auto& primary_video_test_config : primary_video_test_configs) {
    MultipleVideoDecodersTestConfig multiple_video_decoders_test_config;
    for (int i = 0; i < kMaxTotalPlayerInstances; ++i) {
      multiple_video_decoders_test_config.emplace_back(
          primary_video_test_config);
    }
    configs_to_return.emplace_back(multiple_video_decoders_test_config);
  }

  return configs_to_return;
}

void SeekAndDestroy(SbPlayerTestFixture* player_fixture) {
  ASSERT_NO_FATAL_FAILURE(player_fixture->Seek(kSbTimeSecond));
}

void NoInput(SbPlayerTestFixture* player_fixture) {
  GroupedSamples samples;
  if (player_fixture->HasAudio()) {
    samples.AddAudioSamplesWithEOS(0, 0);
  }
  if (player_fixture->HasVideo()) {
    samples.AddVideoSamplesWithEOS(0, 0);
  }
  ASSERT_NO_FATAL_FAILURE(player_fixture->Write(samples));
  ASSERT_NO_FATAL_FAILURE(player_fixture->WaitForPlayerEndOfStream());
}

void WriteSingleBatch(SbPlayerTestFixture* player_fixture) {
  GroupedSamples samples;
  if (player_fixture->HasAudio()) {
    int samples_to_write = SbPlayerGetMaximumNumberOfSamplesPerWrite(
        player_fixture->GetPlayer(), kSbMediaTypeAudio);
    samples.AddAudioSamplesWithEOS(0, samples_to_write);
  }
  if (player_fixture->HasVideo()) {
    int samples_to_write = SbPlayerGetMaximumNumberOfSamplesPerWrite(
        player_fixture->GetPlayer(), kSbMediaTypeVideo);
    samples.AddVideoSamplesWithEOS(0, samples_to_write);
  }

  ASSERT_NO_FATAL_FAILURE(player_fixture->Write(samples));
  ASSERT_NO_FATAL_FAILURE(player_fixture->WaitForPlayerEndOfStream());
}

void WriteMultipleBatches(SbPlayerTestFixture* player_fixture) {
  int samples_to_write = 0;
  // Try to write multiple batches for both audio and video.
  if (player_fixture->HasAudio()) {
    samples_to_write = std::max(
        samples_to_write, SbPlayerGetMaximumNumberOfSamplesPerWrite(
                              player_fixture->GetPlayer(), kSbMediaTypeAudio) +
                              1);
  }
  if (player_fixture->HasVideo()) {
    samples_to_write = std::max(
        samples_to_write, SbPlayerGetMaximumNumberOfSamplesPerWrite(
                              player_fixture->GetPlayer(), kSbMediaTypeVideo) +
                              1);
  }
  // TODO(b/283533109): We'd better to align the written audio and video samples
  // to a same timestamp. Currently, we simply cap the batch size to 8 samples.
  samples_to_write = std::min(samples_to_write, 8);

  GroupedSamples samples;
  if (player_fixture->HasAudio()) {
    samples.AddAudioSamplesWithEOS(0, samples_to_write);
  }
  if (player_fixture->HasVideo()) {
    samples.AddVideoSamplesWithEOS(0, samples_to_write);
  }

  ASSERT_NO_FATAL_FAILURE(player_fixture->Write(samples));
  ASSERT_NO_FATAL_FAILURE(player_fixture->WaitForPlayerEndOfStream());
}

class PlayerThread : public AbstractTestThread {
 public:
  PlayerThread(const SbPlayerTestConfig& config,
               FakeGraphicsContextProvider* fake_graphics_context_provider,
               const std::function<void(SbPlayerTestFixture*)>& functor)
      : config_(config),
        functor_(functor),
        fake_graphics_context_provider_(fake_graphics_context_provider) {}

  void Run() override {
    SbPlayerTestFixture player(config_, fake_graphics_context_provider_);
    functor_(&player);
  }

 private:
  SbPlayerTestConfig config_;
  std::function<void(SbPlayerTestFixture*)> functor_;
  FakeGraphicsContextProvider* fake_graphics_context_provider_;
};

class MultipleVideoDecodersTest
    : public ::testing::TestWithParam<MultipleVideoDecodersTestConfig> {
 protected:
  void RunTest(const std::function<void(SbPlayerTestFixture*)>& functor);
  FakeGraphicsContextProvider fake_graphics_context_provider_;
};

void MultipleVideoDecodersTest::RunTest(
    const std::function<void(SbPlayerTestFixture*)>& functor) {
  const auto& test_configs = GetParam();
  std::vector<std::unique_ptr<PlayerThread>> player_threads;
  for (const auto& test_config : test_configs) {
    player_threads.emplace_back(std::make_unique<PlayerThread>(
        VideoDecodersTestConfigToSbSbPlayerTestConfig(test_config),
        &fake_graphics_context_provider_, functor));
  }
  for (const auto& player_thread : player_threads) {
    player_thread->Start();
  }
  for (const auto& player_thread : player_threads) {
    player_thread->Join();
  }
}

TEST_P(MultipleVideoDecodersTest, SeekAndDestroy) {
  RunTest(SeekAndDestroy);
}

TEST_P(MultipleVideoDecodersTest, NoInput) {
  RunTest(NoInput);
}

TEST_P(MultipleVideoDecodersTest, WriteSingleBatch) {
  RunTest(WriteSingleBatch);
}

TEST_P(MultipleVideoDecodersTest, WriteMultipleBatches) {
  RunTest(WriteMultipleBatches);
}

INSTANTIATE_TEST_CASE_P(MultipleVideoDecodersTests2024,
                        MultipleVideoDecodersTest,
                        ValuesIn(GetSupportedTestConfigs2024()),
                        GetMultipleVideoDecodersTestConfigName);
}  // namespace
}  // namespace nplb
}  // namespace starboard
