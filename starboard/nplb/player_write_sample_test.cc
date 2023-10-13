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

#include "starboard/nplb/drm_helpers.h"
#include "starboard/nplb/player_creation_param_helpers.h"
#include "starboard/nplb/player_test_fixture.h"
#include "starboard/nplb/player_test_util.h"
#include "starboard/nplb/thread_helpers.h"
#include "starboard/string.h"
#include "starboard/testing/fake_graphics_context_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

using ::testing::ValuesIn;

typedef SbPlayerTestFixture::GroupedSamples GroupedSamples;
typedef testing::FakeGraphicsContextProvider FakeGraphicsContextProvider;

class SbPlayerWriteSampleTest
    : public ::testing::TestWithParam<SbPlayerTestConfig> {
 protected:
  FakeGraphicsContextProvider fake_graphics_context_provider_;
};

TEST_P(SbPlayerWriteSampleTest, SeekAndDestroy) {
  SbPlayerTestFixture player_fixture(GetParam(),
                                     &fake_graphics_context_provider_);
  if (HasFatalFailure()) {
    return;
  }
  ASSERT_NO_FATAL_FAILURE(player_fixture.Seek(kSbTimeSecond));
}

TEST_P(SbPlayerWriteSampleTest, NoInput) {
  SbPlayerTestFixture player_fixture(GetParam(),
                                     &fake_graphics_context_provider_);
  if (HasFatalFailure()) {
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

TEST_P(SbPlayerWriteSampleTest, WriteSingleBatch) {
  SbPlayerTestFixture player_fixture(GetParam(),
                                     &fake_graphics_context_provider_);
  if (HasFatalFailure()) {
    return;
  }

  GroupedSamples samples;
  if (player_fixture.HasAudio()) {
    int samples_to_write = SbPlayerGetMaximumNumberOfSamplesPerWrite(
        player_fixture.GetPlayer(), kSbMediaTypeAudio);
    samples.AddAudioSamples(0, samples_to_write);
    samples.AddAudioEOS();
  }
  if (player_fixture.HasVideo()) {
    int samples_to_write = SbPlayerGetMaximumNumberOfSamplesPerWrite(
        player_fixture.GetPlayer(), kSbMediaTypeVideo);
    samples.AddVideoSamples(0, samples_to_write);
    samples.AddVideoEOS();
  }

  ASSERT_NO_FATAL_FAILURE(player_fixture.Write(samples));
  ASSERT_NO_FATAL_FAILURE(player_fixture.WaitForPlayerEndOfStream());
}

TEST_P(SbPlayerWriteSampleTest, WriteMultipleBatches) {
  SbPlayerTestFixture player_fixture(GetParam(),
                                     &fake_graphics_context_provider_);
  if (HasFatalFailure()) {
    return;
  }

  int samples_to_write = 0;
  // Try to write multiple batches for both audio and video.
  if (player_fixture.HasAudio()) {
    samples_to_write = SbPlayerGetMaximumNumberOfSamplesPerWrite(
                           player_fixture.GetPlayer(), kSbMediaTypeAudio) +
                       1;
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
    samples.AddAudioSamples(0, samples_to_write);
    samples.AddAudioEOS();
  }
  if (player_fixture.HasVideo()) {
    samples.AddVideoSamples(0, samples_to_write);
    samples.AddVideoEOS();
  }

  ASSERT_NO_FATAL_FAILURE(player_fixture.Write(samples));
  ASSERT_NO_FATAL_FAILURE(player_fixture.WaitForPlayerEndOfStream());
}

TEST_P(SbPlayerWriteSampleTest, LimitedAudioInput) {
  SbPlayerTestFixture player_fixture(GetParam(),
                                     &fake_graphics_context_provider_);
  if (HasFatalFailure()) {
    return;
  }

  // TODO: we simply set audio write duration to 0.5 second. Ideally, we should
  // set the audio write duration to 10 seconds if audio connectors are remote.
  player_fixture.SetAudioWriteDuration(kSbTimeSecond / 2);

  GroupedSamples samples;
  if (player_fixture.HasAudio()) {
    samples.AddAudioSamples(
        0, player_fixture.ConvertDurationToAudioBufferCount(kSbTimeSecond));
    samples.AddAudioEOS();
  }
  if (player_fixture.HasVideo()) {
    samples.AddVideoSamples(
        0, player_fixture.ConvertDurationToVideoBufferCount(kSbTimeSecond));
    samples.AddVideoEOS();
  }

  ASSERT_NO_FATAL_FAILURE(player_fixture.Write(samples));
  ASSERT_NO_FATAL_FAILURE(player_fixture.WaitForPlayerEndOfStream());
}

TEST_P(SbPlayerWriteSampleTest, PartialAudio) {
  if (!IsPartialAudioSupported()) {
    // TODO: Use GTEST_SKIP when we have a newer version of gtest.
    SB_LOG(INFO)
        << "The platform doesn't support partial audio. Skip the tests.";
    return;
  }

  SbPlayerTestFixture player_fixture(GetParam(),
                                     &fake_graphics_context_provider_);
  if (HasFatalFailure()) {
    return;
  }
  if (!player_fixture.HasAudio()) {
    // TODO: Use GTEST_SKIP when we have a newer version of gtest.
    SB_LOG(INFO) << "Skip PartialAudio test for audioless content.";
    return;
  }

  const SbTime kDurationToPlay = kSbTimeSecond;
  const float kSegmentSize = 0.1f;

  GroupedSamples samples;
  if (player_fixture.HasVideo()) {
    samples.AddVideoSamples(
        0, player_fixture.ConvertDurationToVideoBufferCount(kDurationToPlay));
    samples.AddVideoEOS();
  }

  int total_buffers_to_write =
      player_fixture.ConvertDurationToAudioBufferCount(kDurationToPlay);
  for (int i = 0; i < total_buffers_to_write; i++) {
    SbTime current_timestamp = player_fixture.GetAudioSampleTimestamp(i);
    SbTime next_timestamp = player_fixture.GetAudioSampleTimestamp(i + 1);
    SbTime buffer_duration = next_timestamp - current_timestamp;
    SbTime segment_duration = buffer_duration * kSegmentSize;
    SbTime written_duration = 0;
    while (written_duration < buffer_duration) {
      samples.AddAudioSamples(
          i, 1, written_duration, written_duration,
          std::max<int64_t>(
              0, buffer_duration - written_duration - segment_duration));
      written_duration += segment_duration;
    }
  }
  samples.AddAudioEOS();

  ASSERT_NO_FATAL_FAILURE(player_fixture.Write(samples));
  ASSERT_NO_FATAL_FAILURE(player_fixture.WaitForPlayerPresenting());

  SbTime start_system_time = SbTimeGetMonotonicNow();
  SbTime start_media_time = player_fixture.GetCurrentMediaTime();

  ASSERT_NO_FATAL_FAILURE(player_fixture.WaitForPlayerEndOfStream());

  SbTime end_system_time = SbTimeGetMonotonicNow();
  SbTime end_media_time = player_fixture.GetCurrentMediaTime();

  const SbTime kDurationDifferenceAllowance = 500 * kSbTimeMillisecond;
  EXPECT_NEAR(end_media_time, kDurationToPlay, kDurationDifferenceAllowance);
  EXPECT_NEAR(end_system_time - start_system_time + start_media_time,
              kDurationToPlay, kDurationDifferenceAllowance);

  SB_DLOG(INFO) << "The expected media time should be " << kDurationToPlay
                << ", the actual media time is " << end_media_time
                << ", with difference "
                << std::abs(end_media_time - kDurationToPlay) << ".";
  SB_DLOG(INFO) << "The expected total playing time should be "
                << kDurationToPlay << ", the actual playing time is "
                << end_system_time - start_system_time + start_media_time
                << ", with difference "
                << std::abs(end_system_time - start_system_time +
                            start_media_time - kDurationToPlay)
                << ".";
}

TEST_P(SbPlayerWriteSampleTest, PartialAudioDiscardAll) {
  if (!IsPartialAudioSupported()) {
    // TODO: Use GTEST_SKIP when we have a newer version of gtest.
    SB_LOG(INFO)
        << "The platform doesn't support partial audio. Skip the tests.";
    return;
  }

  SbPlayerTestFixture player_fixture(GetParam(),
                                     &fake_graphics_context_provider_);
  if (HasFatalFailure()) {
    return;
  }
  if (!player_fixture.HasAudio()) {
    // TODO: Use GTEST_SKIP when we have a newer version of gtest.
    SB_LOG(INFO) << "Skip PartialAudio test for audioless content.";
    return;
  }

  const SbTime kDurationToPlay = kSbTimeSecond;
  const SbTime kDurationPerWrite = 100 * kSbTimeMillisecond;
  const SbTime kNumberOfBuffersToDiscard = 20;

  GroupedSamples samples;
  if (player_fixture.HasVideo()) {
    samples.AddVideoSamples(
        0, player_fixture.ConvertDurationToVideoBufferCount(kDurationToPlay));
    samples.AddVideoEOS();
  }

  int written_buffer_index = 0;
  SbTime current_time_offset = 0;
  int num_of_buffers_per_write =
      player_fixture.ConvertDurationToAudioBufferCount(kDurationPerWrite);
  int count = 0;
  while (current_time_offset < kDurationToPlay) {
    const SbTime kDurationToDiscard =
        count % 2 == 0 ? kSbTimeSecond : kSbTimeMax;
    count++;
    // Discard from front.
    for (int i = 0; i < kNumberOfBuffersToDiscard; i++) {
      samples.AddAudioSamples(written_buffer_index, 1, current_time_offset,
                              kDurationToDiscard, 0);
    }

    samples.AddAudioSamples(written_buffer_index, num_of_buffers_per_write);
    written_buffer_index += num_of_buffers_per_write;
    current_time_offset += kDurationPerWrite;

    // Discard from back.
    for (int i = 0; i < kNumberOfBuffersToDiscard; i++) {
      samples.AddAudioSamples(written_buffer_index, 1, current_time_offset, 0,
                              kDurationToDiscard);
    }
  }
  samples.AddAudioEOS();

  ASSERT_NO_FATAL_FAILURE(player_fixture.Write(samples));
  ASSERT_NO_FATAL_FAILURE(player_fixture.WaitForPlayerPresenting());

  SbTime start_system_time = SbTimeGetMonotonicNow();
  SbTime start_media_time = player_fixture.GetCurrentMediaTime();

  ASSERT_NO_FATAL_FAILURE(player_fixture.WaitForPlayerEndOfStream());

  SbTime end_system_time = SbTimeGetMonotonicNow();
  SbTime end_media_time = player_fixture.GetCurrentMediaTime();

  const SbTime kDurationDifferenceAllowance = 500 * kSbTimeMillisecond;
  SbTime total_written_duration =
      player_fixture.GetAudioSampleTimestamp(written_buffer_index);
  EXPECT_NEAR(end_media_time, total_written_duration,
              kDurationDifferenceAllowance);
  EXPECT_NEAR(end_system_time - start_system_time + start_media_time,
              total_written_duration, kDurationDifferenceAllowance);

  SB_DLOG(INFO) << "The expected media time should be "
                << total_written_duration << ", the actual media time is "
                << end_media_time << ", with difference "
                << std::abs(end_media_time - total_written_duration) << ".";
  SB_DLOG(INFO) << "The expected total playing time should be "
                << total_written_duration << ", the actual playing time is "
                << end_system_time - start_system_time + start_media_time
                << ", with difference "
                << std::abs(end_system_time - start_system_time +
                            start_media_time - total_written_duration)
                << ".";
}

class SecondaryPlayerTestThread : public AbstractTestThread {
 public:
  SecondaryPlayerTestThread(
      const SbPlayerTestConfig& config,
      FakeGraphicsContextProvider* fake_graphics_context_provider)
      : config_(config),
        fake_graphics_context_provider_(fake_graphics_context_provider) {}

  void Run() override {
    SbPlayerTestFixture player_fixture(config_,
                                       fake_graphics_context_provider_);
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

 private:
  const SbPlayerTestConfig config_;
  FakeGraphicsContextProvider* fake_graphics_context_provider_;
};

TEST_P(SbPlayerWriteSampleTest, SecondaryPlayerTest) {
  // The secondary player should at least support h264 at 480p, 30fps and with
  // a drm system.
  const char* kMaxVideoCapabilities = "width=640; height=480; framerate=30;";
  const char* kSecondaryTestFile = "beneath_the_canopy_135_avc.dmp";
  const char* key_system = GetParam().key_system;

  SbPlayerTestConfig secondary_player_config(nullptr, kSecondaryTestFile,
                                             kSbPlayerOutputModeInvalid,
                                             key_system, kMaxVideoCapabilities);
  PlayerCreationParam creation_param =
      CreatePlayerCreationParam(secondary_player_config);
  secondary_player_config.output_mode = GetPreferredOutputMode(creation_param);

  ASSERT_NE(secondary_player_config.output_mode, kSbPlayerOutputModeInvalid);

  SecondaryPlayerTestThread primary_player_thread(
      GetParam(), &fake_graphics_context_provider_);
  SecondaryPlayerTestThread secondary_player_thread(
      secondary_player_config, &fake_graphics_context_provider_);

  primary_player_thread.Start();
  secondary_player_thread.Start();

  primary_player_thread.Join();
  secondary_player_thread.Join();
}

std::vector<SbPlayerTestConfig> GetSupportedTestConfigs() {
  static std::vector<SbPlayerTestConfig> supported_configs;
  if (supported_configs.size() > 0) {
    return supported_configs;
  }

  const std::vector<const char*>& key_systems = GetKeySystems();
  for (auto key_system : key_systems) {
    std::vector<SbPlayerTestConfig> configs =
        GetSupportedSbPlayerTestConfigs(key_system);
    supported_configs.insert(supported_configs.end(), configs.begin(),
                             configs.end());
  }
  return supported_configs;
}

INSTANTIATE_TEST_CASE_P(SbPlayerWriteSampleTests,
                        SbPlayerWriteSampleTest,
                        ValuesIn(GetSupportedTestConfigs()),
                        GetSbPlayerTestConfigName);

}  // namespace
}  // namespace nplb
}  // namespace starboard
