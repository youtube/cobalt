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

#include "starboard/common/string.h"
#include "starboard/nplb/player_test_fixture.h"
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

std::string GetTestConfigName(const SbPlayerTestConfig& config) {
  std::string name;

  if (std::get<0>(config)) {
    name += FormatString("audio_%s_", std::get<0>(config));
  }
  if (std::get<1>(config)) {
    name += FormatString("video_%s_", std::get<1>(config));
  }
  return name;
}

std::map<SbPlayerTestConfig, int> TestConfigsToMap(
    const SbPlayerMultiplePlayerTestConfig& test_configs) {
  std::map<SbPlayerTestConfig, int> map;
  for (const auto& test_config : test_configs) {
    ++map[test_config];
  }
  return map;
}

std::string GetMultipleSbPlayerTestConfigName(
    ::testing::TestParamInfo<SbPlayerMultiplePlayerTestConfig> info) {
  const auto test_configs = info.param;
  std::string name;

  const auto& map = TestConfigsToMap(test_configs);
  for (const auto& it : map) {
    name += GetTestConfigName(it.first);
    name += "num_";
    name += std::to_string(it.second);
  }
  name +=
      (std::get<2>(test_configs.front()) == kSbPlayerOutputModeDecodeToTexture
           ? "_DecodeToTexture"
           : "_Punchout");
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

class MultiplePlayerTest
    : public ::testing::TestWithParam<SbPlayerMultiplePlayerTestConfig> {
 protected:
  void RunTest(const std::function<void(SbPlayerTestFixture*)>& functor);
  FakeGraphicsContextProvider fake_graphics_context_provider_;
};

void MultiplePlayerTest::RunTest(
    const std::function<void(SbPlayerTestFixture*)>& functor) {
  const auto& test_configs = GetParam();

  std::vector<std::unique_ptr<PlayerThread>> player_threads;
  for (const auto& test_config : test_configs) {
    player_threads.emplace_back(std::make_unique<PlayerThread>(
        test_config, &fake_graphics_context_provider_, functor));
  }
  for (const auto& player_thread : player_threads) {
    player_thread->Start();
  }
  for (const auto& player_thread : player_threads) {
    player_thread->Join();
  }
}

TEST_P(MultiplePlayerTest, NoInput) {
  RunTest(NoInput);
}

TEST_P(MultiplePlayerTest, SeekAndDestroy) {
  RunTest(SeekAndDestroy);
}

TEST_P(MultiplePlayerTest, WriteSingleBatch) {
  RunTest(WriteSingleBatch);
}

TEST_P(MultiplePlayerTest, WriteMultipleBatches) {
  RunTest(WriteMultipleBatches);
}

INSTANTIATE_TEST_CASE_P(MultiplePlayerTests,
                        MultiplePlayerTest,
                        ValuesIn(GetSupportedTestConfigs()),
                        GetMultipleSbPlayerTestConfigName);

}  // namespace
}  // namespace nplb
}  // namespace starboard
