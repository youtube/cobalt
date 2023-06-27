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
#include "starboard/nplb/player_test_fixture.h"
#include "starboard/nplb/player_test_util.h"
#include "starboard/nplb/thread_helpers.h"
#include "starboard/testing/fake_graphics_context_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

using ::starboard::testing::FakeGraphicsContextProvider;
using ::testing::ValuesIn;

typedef SbPlayerTestFixture::GroupedSamples GroupedSamples;
typedef std::vector<SbPlayerTestConfig> SbPlayerMultiplePlayerTestConfig;
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

std::vector<SbPlayerMultiplePlayerTestConfig> GetSupportedTestConfigs() {
  std::vector<SbPlayerMultiplePlayerTestConfig> configs_to_return;
  // TODO(cweichun): wait for maximum configuration explorer, set to 1 now.
  const int num_of_players = 1;
  std::vector<SbPlayerTestConfig> supported_configs =
      GetSupportedSbPlayerTestConfigs();
  for (auto& config : supported_configs) {
    SbPlayerMultiplePlayerTestConfig multiplayer_test_config(num_of_players,
                                                             config);
    configs_to_return.emplace_back(multiplayer_test_config);
  }
  return configs_to_return;
}

INSTANTIATE_TEST_CASE_P(MultiplePlayerTests,
                        MultiplePlayerTest,
                        ValuesIn(GetSupportedTestConfigs()),
                        GetMultipleSbPlayerTestConfigName);

}  // namespace
}  // namespace nplb
}  // namespace starboard
