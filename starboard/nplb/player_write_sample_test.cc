// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/common/string.h"
#include "starboard/nplb/drm_helpers.h"
#include "starboard/nplb/player_test_fixture.h"
#include "starboard/nplb/player_test_util.h"
#include "starboard/string.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

using ::testing::ValuesIn;

typedef SbPlayerTestFixture::GroupedSamples GroupedSamples;

class SbPlayerWriteSampleTest
    : public ::testing::TestWithParam<SbPlayerTestConfig> {};

TEST_P(SbPlayerWriteSampleTest, SeekAndDestroy) {
  SbPlayerTestFixture player_fixture(GetParam());
  if (HasFatalFailure()) {
    return;
  }
  ASSERT_NO_FATAL_FAILURE(player_fixture.Seek(kSbTimeSecond));
}

TEST_P(SbPlayerWriteSampleTest, NoInput) {
  SbPlayerTestFixture player_fixture(GetParam());
  if (HasFatalFailure()) {
    return;
  }

  GroupedSamples samples;
  if (player_fixture.HasAudio()) {
    samples.AddAudioSamplesWithEOS(0, 0);
  }
  if (player_fixture.HasVideo()) {
    samples.AddVideoSamplesWithEOS(0, 0);
  }
  ASSERT_NO_FATAL_FAILURE(player_fixture.Write(samples));
  ASSERT_NO_FATAL_FAILURE(player_fixture.WaitForPlayerEndOfStream());
}

TEST_P(SbPlayerWriteSampleTest, WriteSingleBatch) {
  SbPlayerTestFixture player_fixture(GetParam());
  if (HasFatalFailure()) {
    return;
  }

  GroupedSamples samples;
  if (player_fixture.HasAudio()) {
    int samples_to_write = SbPlayerGetMaximumNumberOfSamplesPerWrite(
        player_fixture.GetPlayer(), kSbMediaTypeAudio);
    samples.AddAudioSamplesWithEOS(0, samples_to_write);
  }
  if (player_fixture.HasVideo()) {
    int samples_to_write = SbPlayerGetMaximumNumberOfSamplesPerWrite(
        player_fixture.GetPlayer(), kSbMediaTypeVideo);
    samples.AddVideoSamplesWithEOS(0, samples_to_write);
  }

  ASSERT_NO_FATAL_FAILURE(player_fixture.Write(samples));
  ASSERT_NO_FATAL_FAILURE(player_fixture.WaitForPlayerEndOfStream());
}

TEST_P(SbPlayerWriteSampleTest, WriteMultipleBatches) {
  SbPlayerTestFixture player_fixture(GetParam());
  if (HasFatalFailure()) {
    return;
  }

  int samples_to_write = 0;
  // Try to write multiple batches for both audio and video.
  if (player_fixture.HasAudio()) {
    samples_to_write = std::max(
        samples_to_write, SbPlayerGetMaximumNumberOfSamplesPerWrite(
                              player_fixture.GetPlayer(), kSbMediaTypeAudio) +
                              1);
  }
  if (player_fixture.HasVideo()) {
    samples_to_write = std::max(
        samples_to_write, SbPlayerGetMaximumNumberOfSamplesPerWrite(
                              player_fixture.GetPlayer(), kSbMediaTypeVideo) +
                              1);
  }
  // TODO(b/283533109): We'd better to align the written audio and video samples
  // to a same timestamp. Currently, we simply cap the batch size to 8 samples.
  samples_to_write = std::min(samples_to_write, 8);

  GroupedSamples samples;
  if (player_fixture.HasAudio()) {
    samples.AddAudioSamplesWithEOS(0, samples_to_write);
  }
  if (player_fixture.HasVideo()) {
    samples.AddVideoSamplesWithEOS(0, samples_to_write);
  }

  ASSERT_NO_FATAL_FAILURE(player_fixture.Write(samples));
  ASSERT_NO_FATAL_FAILURE(player_fixture.WaitForPlayerEndOfStream());
}

std::vector<SbPlayerTestConfig> GetSupportedTestConfigs() {
  static std::vector<SbPlayerTestConfig> supported_configs;
  if (supported_configs.size() > 0) {
    return supported_configs;
  }

  std::vector<const char*> key_systems;
  key_systems.push_back("");
  key_systems.insert(key_systems.end(), kKeySystems,
                     kKeySystems + SB_ARRAY_SIZE_INT(kKeySystems));

  for (auto key_system : key_systems) {
    std::vector<SbPlayerTestConfig> configs =
        GetSupportedSbPlayerTestConfigs(key_system);
    supported_configs.insert(supported_configs.end(), configs.begin(),
                             configs.end());
  }
  return supported_configs;
}

std::string GetTestConfigName(
    ::testing::TestParamInfo<SbPlayerTestConfig> info) {
  const SbPlayerTestConfig& config = info.param;
  const char* audio_filename = std::get<0>(config);
  const char* video_filename = std::get<1>(config);
  const SbPlayerOutputMode output_mode = std::get<2>(config);
  const char* key_system = std::get<3>(config);
  std::string name(FormatString(
      "audio_%s_video_%s_output_%s_key_system_%s",
      audio_filename && strlen(audio_filename) > 0 ? audio_filename : "null",
      video_filename && strlen(video_filename) > 0 ? video_filename : "null",
      output_mode == kSbPlayerOutputModeDecodeToTexture ? "decode_to_texture"
                                                        : "punch_out",
      strlen(key_system) > 0 ? key_system : "null"));
  std::replace(name.begin(), name.end(), '.', '_');
  std::replace(name.begin(), name.end(), '(', '_');
  std::replace(name.begin(), name.end(), ')', '_');
  return name;
}

INSTANTIATE_TEST_CASE_P(SbPlayerWriteSampleTests,
                        SbPlayerWriteSampleTest,
                        ValuesIn(GetSupportedTestConfigs()),
                        GetTestConfigName);

}  // namespace
}  // namespace nplb
}  // namespace starboard
