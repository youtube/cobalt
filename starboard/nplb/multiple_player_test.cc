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

#include "build/build_config.h"
#include "starboard/common/check_op.h"
#include "starboard/common/string.h"
#include "starboard/nplb/maximum_player_configuration_explorer.h"
#include "starboard/nplb/player_test_fixture.h"
#include "starboard/nplb/player_test_util.h"
#include "starboard/nplb/posix_compliance/posix_thread_helpers.h"
#include "starboard/testing/fake_graphics_context_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_IOS_TVOS)
#include "starboard/tvos/shared/run_in_background_thread_and_wait.h"
#endif  // BUILDFLAG(IS_IOS_TVOS)

namespace nplb {
namespace {

using ::starboard::FakeGraphicsContextProvider;
using ::starboard::VideoDmpReader;
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

  // A wrapper for Join() that, on tvOS, invokes it on a background GCD queue.
  // This must be done to avoid a deadlock when the main thread is blocked on
  // Join() and the worker thread is blocked attempting to invoke code in the
  // main thread (in AVSBVideoRenderer).
  void WaitForFinish() {
#if BUILDFLAG(IS_IOS_TVOS)
    RunInBackgroundThreadAndWait([this] { Join(); });
#else
    Join();
#endif  // BUILDFLAG(IS_IOS_TVOS)
  }

 private:
  std::function<void()> functor_;
};

std::string GetMultipleSbPlayerTestConfigDescription(
    SbPlayerMultiplePlayerTestConfig multiplayer_test_config) {
  SB_DCHECK_GT(multiplayer_test_config.size(), static_cast<size_t>(0));
  const SbPlayerOutputMode output_mode = multiplayer_test_config[0].output_mode;
  const char* key_system = multiplayer_test_config[0].key_system;

  std::string description;
  for (size_t i = 0; i < multiplayer_test_config.size(); i++) {
    const SbPlayerTestConfig& config = multiplayer_test_config[i];
    const char* audio_filename = config.audio_filename;
    const char* video_filename = config.video_filename;

    if (i > 0) {
      description += "_";
    }
    description += starboard::FormatString(
        "audio%zu_%s_video%zu_%s", i,
        audio_filename && strlen(audio_filename) > 0 ? audio_filename : "null",
        i,
        video_filename && strlen(video_filename) > 0 ? video_filename : "null");
  }

  description += starboard::FormatString(
      "_output_%s_key_system_%s",
      output_mode == kSbPlayerOutputModeDecodeToTexture ? "decode_to_texture"
                                                        : "punch_out",
      strlen(key_system) > 0 ? key_system : "null");
  std::replace(description.begin(), description.end(), '.', '_');
  std::replace(description.begin(), description.end(), '(', '_');
  std::replace(description.begin(), description.end(), ')', '_');
  return description;
}

class MultiplePlayerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const int kMaxPlayerInstancesPerConfig = 3;
    const int kMaxTotalPlayerInstances = 5;

    const std::vector<const char*>& audio_test_files =
        GetStereoAudioTestFiles();
    const std::vector<const char*>& video_test_files = GetVideoTestFiles();
    const std::vector<SbPlayerOutputMode>& output_modes =
        GetPlayerOutputModes();
    const std::vector<const char*>& key_systems = GetKeySystems();

    for (auto key_system : key_systems) {
      std::vector<SbPlayerTestConfig> supported_configs;
      for (auto video_filename : video_test_files) {
        VideoDmpReader dmp_reader(video_filename,
                                  VideoDmpReader::kEnableReadOnDemand);
        SB_DCHECK_GT(dmp_reader.number_of_video_buffers(),
                     static_cast<size_t>(0));
        if (SbMediaCanPlayMimeAndKeySystem(dmp_reader.video_mime_type().c_str(),
                                           key_system)) {
          supported_configs.push_back({nullptr, video_filename,
                                       kSbPlayerOutputModeInvalid, key_system});
        }
      }

      if (supported_configs.empty()) {
        continue;
      }

      std::vector<const char*> supported_audio_files;
      for (auto audio_filename : audio_test_files) {
        VideoDmpReader dmp_reader(audio_filename,
                                  VideoDmpReader::kEnableReadOnDemand);
        SB_DCHECK_GT(dmp_reader.number_of_audio_buffers(),
                     static_cast<size_t>(0));
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
            kMaxTotalPlayerInstances, &fake_graphics_context_provider_);
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

        multiplayer_test_configs_.insert(multiplayer_test_configs_.end(),
                                         explorer_output.begin(),
                                         explorer_output.end());
      }
    }
  }

  void RunTest(const MultiplePlayerTestFunctor& functor,
               const SbPlayerMultiplePlayerTestConfig& multiplayer_test_config);

  std::vector<SbPlayerMultiplePlayerTestConfig> multiplayer_test_configs_;
  FakeGraphicsContextProvider fake_graphics_context_provider_;
};

void MultiplePlayerTest::RunTest(
    const MultiplePlayerTestFunctor& functor,
    const SbPlayerMultiplePlayerTestConfig& multiplayer_test_config) {
  std::list<PlayerThread> player_threads;
  for (const SbPlayerTestConfig& player_config : multiplayer_test_config) {
    player_threads.emplace_back(
        std::bind(functor, player_config, &fake_graphics_context_provider_));
  }
  for (auto& player_thread : player_threads) {
    player_thread.Start();
  }
  for (auto& player_thread : player_threads) {
    player_thread.WaitForFinish();
  }
}

void WriteSamples(const SbPlayerTestConfig& player_config,
                  FakeGraphicsContextProvider* fake_graphics_context_provider) {
  SbPlayerTestFixture player_fixture(player_config,
                                     fake_graphics_context_provider);
  if (::testing::Test::HasFatalFailure()) {
    return;
  }

  const int64_t kDurationToPlay = 200'000;  // 200ms

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

TEST_F(MultiplePlayerTest, SunnyDay) {
  for (auto multiplayer_test_config : multiplayer_test_configs_) {
    SB_LOG(INFO) << "Start testing: "
                 << GetMultipleSbPlayerTestConfigDescription(
                        multiplayer_test_config);
    RunTest(WriteSamples, multiplayer_test_config);
  }
}

}  // namespace
}  // namespace nplb
