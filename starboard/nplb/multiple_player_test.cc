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

#include <list>
#include <string>

#include "starboard/common/string.h"
#include "starboard/nplb/maximum_player_configuration_explorer.h"
#include "starboard/nplb/player_test_fixture.h"
#include "starboard/nplb/player_test_util.h"
#include "starboard/nplb/thread_helpers.h"
#include "starboard/testing/fake_graphics_context_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

using shared::starboard::player::video_dmp::VideoDmpReader;
using ::starboard::testing::FakeGraphicsContextProvider;
using ::testing::ValuesIn;

typedef SbPlayerTestFixture::GroupedSamples GroupedSamples;
typedef std::function<void(const SbPlayerTestConfig&,
                           FakeGraphicsContextProvider*)>
    MultiplePlayerTestFunctor;

class PlayerThread : public AbstractTestThread {
 public:
  explicit PlayerThread(const std::function<void()>& functor)
      : functor_(functor) {}

  void Run() override { functor_(); }

 private:
  std::function<void()> functor_;
};

class MultiplePlayerTest
    : public ::testing::TestWithParam<SbPlayerMultiplePlayerTestConfig> {
 protected:
  void RunTest(const MultiplePlayerTestFunctor& functor);
  FakeGraphicsContextProvider fake_graphics_context_provider_;
};

void MultiplePlayerTest::RunTest(const MultiplePlayerTestFunctor& functor) {
  const SbPlayerMultiplePlayerTestConfig& multiplayer_test_config = GetParam();
  std::list<PlayerThread> player_threads;
  for (const SbPlayerTestConfig& player_config : multiplayer_test_config) {
    player_threads.emplace_back(
        std::bind(functor, player_config, &fake_graphics_context_provider_));
  }
  for (auto& player_thread : player_threads) {
    player_thread.Start();
  }
  for (auto& player_thread : player_threads) {
    player_thread.Join();
  }
}

void NoInput(const SbPlayerTestConfig& player_config,
             FakeGraphicsContextProvider* fake_graphics_context_provider) {
  SbPlayerTestFixture player_fixture(player_config,
                                     fake_graphics_context_provider);
  if (::testing::Test::HasFatalFailure()) {
    return;
  }

  GroupedSamples samples;
  if (player_fixture.HasAudio()) {
    samples.AddAudioEOS();
  }
  if (player_fixture.HasVideo()) {
    samples.AddVideoEOS();
  }
  ASSERT_NO_FATAL_FAILURE(player_fixture.Write(samples));
  ASSERT_NO_FATAL_FAILURE(player_fixture.WaitForPlayerEndOfStream());
}

TEST_P(MultiplePlayerTest, NoInput) {
  RunTest(NoInput);
}

void WriteSamples(const SbPlayerTestConfig& player_config,
                  FakeGraphicsContextProvider* fake_graphics_context_provider) {
  SbPlayerTestFixture player_fixture(player_config,
                                     fake_graphics_context_provider);
  if (::testing::Test::HasFatalFailure()) {
    return;
  }

  const SbTime kDurationToPlay = kSbTimeMillisecond * 200;

  GroupedSamples samples;
  if (player_fixture.HasAudio()) {
    samples.AddAudioSamples(
        0, player_fixture.ConvertDurationToAudioBufferCount(kDurationToPlay));
    samples.AddAudioEOS();
  }
  if (player_fixture.HasVideo()) {
    samples.AddVideoSamples(
        0, player_fixture.ConvertDurationToVideoBufferCount(kDurationToPlay));
    samples.AddVideoEOS();
  }

  ASSERT_NO_FATAL_FAILURE(player_fixture.Write(samples));
  ASSERT_NO_FATAL_FAILURE(player_fixture.WaitForPlayerEndOfStream());
}

TEST_P(MultiplePlayerTest, WriteSamples) {
  RunTest(WriteSamples);
}

std::string GetMultipleSbPlayerTestConfigName(
    ::testing::TestParamInfo<SbPlayerMultiplePlayerTestConfig> info) {
  const SbPlayerMultiplePlayerTestConfig& multiplayer_test_config = info.param;

  SB_DCHECK(multiplayer_test_config.size() > 0);
  const SbPlayerOutputMode output_mode = multiplayer_test_config[0].output_mode;
  const char* key_system = multiplayer_test_config[0].key_system;

  std::string name;
  for (int i = 0; i < multiplayer_test_config.size(); i++) {
    const SbPlayerTestConfig& config = multiplayer_test_config[i];
    const char* audio_filename = config.audio_filename;
    const char* video_filename = config.video_filename;

    if (i > 0) {
      name += "_";
    }
    name += FormatString(
        "audio%d_%s_video%d_%s", i,
        audio_filename && strlen(audio_filename) > 0 ? audio_filename : "null",
        i,
        video_filename && strlen(video_filename) > 0 ? video_filename : "null");
  }

  name += FormatString("_output_%s_key_system_%s",
                       output_mode == kSbPlayerOutputModeDecodeToTexture
                           ? "decode_to_texture"
                           : "punch_out",
                       strlen(key_system) > 0 ? key_system : "null");
  std::replace(name.begin(), name.end(), '.', '_');
  std::replace(name.begin(), name.end(), '(', '_');
  std::replace(name.begin(), name.end(), ')', '_');
  return name;
}

std::vector<SbPlayerMultiplePlayerTestConfig> GetMultiplePlayerTestConfigs() {
  const int kMaxPlayerInstancesPerConfig = 3;
  const int kMaxTotalPlayerInstances = 5;

  const std::vector<const char*>& audio_test_files = GetAudioTestFiles();
  const std::vector<const char*>& video_test_files = GetVideoTestFiles();
  const std::vector<SbPlayerOutputMode>& output_modes = GetPlayerOutputModes();
  const std::vector<const char*>& key_systems = GetKeySystems();

  FakeGraphicsContextProvider fake_graphics_context_provider;
  std::vector<SbPlayerMultiplePlayerTestConfig> configs_to_return;

  for (auto key_system : key_systems) {
    std::vector<SbPlayerTestConfig> supported_configs;
    for (auto video_filename : video_test_files) {
      VideoDmpReader dmp_reader(video_filename,
                                VideoDmpReader::kEnableReadOnDemand);
      SB_DCHECK(dmp_reader.number_of_video_buffers() > 0);
      if (SbMediaCanPlayMimeAndKeySystem(dmp_reader.video_mime_type().c_str(),
                                         key_system)) {
        supported_configs.push_back(
            {nullptr, video_filename, kSbPlayerOutputModeInvalid, key_system});
      }
    }

    if (supported_configs.empty()) {
      continue;
    }

    std::vector<const char*> supported_audio_files;
    for (auto audio_filename : audio_test_files) {
      VideoDmpReader dmp_reader(audio_filename,
                                VideoDmpReader::kEnableReadOnDemand);
      SB_DCHECK(dmp_reader.number_of_audio_buffers() > 0);
      if (SbMediaCanPlayMimeAndKeySystem(dmp_reader.audio_mime_type().c_str(),
                                         key_system)) {
        supported_audio_files.push_back(audio_filename);
      }
    }

    // TODO: use SbPlayerGetPreferredOutputMode() to choose output mode.
    for (auto output_mode : output_modes) {
      for (auto& config : supported_configs) {
        config.output_mode = output_mode;
      }

      MaximumPlayerConfigurationExplorer explorer(
          supported_configs, kMaxPlayerInstancesPerConfig,
          kMaxTotalPlayerInstances, &fake_graphics_context_provider);
      std::vector<SbPlayerMultiplePlayerTestConfig> explorer_output =
          explorer.CalculateMaxTestConfigs();

      // Add audio codec to configs using round robin algorithm.
      for (auto& multi_player_test_config : explorer_output) {
        int audio_file_index = 0;
        for (auto& config : multi_player_test_config) {
          config.audio_filename = supported_audio_files[audio_file_index];
          audio_file_index =
              (audio_file_index + 1) % supported_audio_files.size();
        }
      }

      configs_to_return.insert(configs_to_return.end(), explorer_output.begin(),
                               explorer_output.end());
    }
  }
  return configs_to_return;
}

INSTANTIATE_TEST_CASE_P(MultiplePlayerTests,
                        MultiplePlayerTest,
                        ValuesIn(GetMultiplePlayerTestConfigs()),
                        GetMultipleSbPlayerTestConfigName);

}  // namespace
}  // namespace nplb
}  // namespace starboard
