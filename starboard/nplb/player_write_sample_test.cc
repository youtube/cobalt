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

#include <limits>

#include "starboard/common/time.h"
#include "starboard/nplb/drm_helpers.h"
#include "starboard/nplb/player_creation_param_helpers.h"
#include "starboard/nplb/player_test_fixture.h"
#include "starboard/nplb/player_test_util.h"
#include "starboard/nplb/posix_compliance/posix_thread_helpers.h"
#include "starboard/testing/fake_graphics_context_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

using ::starboard::FakeGraphicsContextProvider;
using ::testing::ValuesIn;

using GroupedSamples = SbPlayerTestFixture::GroupedSamples;

class SbPlayerWriteSampleTest
    : public ::testing::TestWithParam<SbPlayerTestConfig> {
 protected:
  void SetUp() override { SkipTestIfNotSupported(GetParam()); }

  FakeGraphicsContextProvider fake_graphics_context_provider_;
};

TEST_P(SbPlayerWriteSampleTest, SeekAndDestroy) {
  SbPlayerTestFixture player_fixture(GetParam(),
                                     &fake_graphics_context_provider_);
  if (HasFatalFailure()) {
    return;
  }
  ASSERT_NO_FATAL_FAILURE(player_fixture.Seek(1'000'000));
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
  // TODO: b/347728473 When the platform supports multiple
  // samples per write to SbPlayer, the following numbers are
  // sufficient to allow SbPlayer to play 60fps video.
  // Revisit the maximum numbers if the requirement is changed.
  const int kMaxAudioSamplesPerWrite = 15;
  const int kMaxVideoSamplesPerWrite = 60;

  SbPlayerTestFixture player_fixture(GetParam(),
                                     &fake_graphics_context_provider_);
  if (HasFatalFailure()) {
    return;
  }

  GroupedSamples samples;
  if (player_fixture.HasAudio()) {
    int samples_to_write =
        std::min(SbPlayerGetMaximumNumberOfSamplesPerWrite(
                     player_fixture.GetPlayer(), kSbMediaTypeAudio),
                 kMaxAudioSamplesPerWrite);
    samples.AddAudioSamples(0, samples_to_write);
    samples.AddAudioEOS();
  }
  if (player_fixture.HasVideo()) {
    int samples_to_write =
        std::min(SbPlayerGetMaximumNumberOfSamplesPerWrite(
                     player_fixture.GetPlayer(), kSbMediaTypeVideo),
                 kMaxVideoSamplesPerWrite);
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
  player_fixture.SetAudioWriteDuration(500'000);

  GroupedSamples samples;
  if (player_fixture.HasAudio()) {
    samples.AddAudioSamples(
        0, player_fixture.ConvertDurationToAudioBufferCount(1'000'000));
    samples.AddAudioEOS();
  }
  if (player_fixture.HasVideo()) {
    samples.AddVideoSamples(
        0, player_fixture.ConvertDurationToVideoBufferCount(1'000'000));
    samples.AddVideoEOS();
  }

  ASSERT_NO_FATAL_FAILURE(player_fixture.Write(samples));
  ASSERT_NO_FATAL_FAILURE(player_fixture.WaitForPlayerEndOfStream());
}

TEST_P(SbPlayerWriteSampleTest, PartialAudio) {
  if (!IsPartialAudioSupported()) {
    GTEST_SKIP() << "The platform doesn't support partial audio.";
  }
  if (IsAudioPassthroughUsed(GetParam())) {
    GTEST_SKIP() << "The audio passthrough doesn't support partial audio.";
  }

  SbPlayerTestFixture player_fixture(GetParam(),
                                     &fake_graphics_context_provider_);
  if (HasFatalFailure()) {
    return;
  }
  if (!player_fixture.HasAudio()) {
    GTEST_SKIP() << "Skip PartialAudio test for audioless content.";
  }

  const int64_t kDurationToPlay = 1'000'000;  // 1 second
  const float kSegmentSize = 0.3f;

  GroupedSamples samples;
  if (player_fixture.HasVideo()) {
    samples.AddVideoSamples(
        0, player_fixture.ConvertDurationToVideoBufferCount(kDurationToPlay));
    samples.AddVideoEOS();
  }

  int total_buffers_to_write =
      player_fixture.ConvertDurationToAudioBufferCount(kDurationToPlay);
  for (int i = 0; i < total_buffers_to_write; i++) {
    int64_t current_timestamp = player_fixture.GetAudioSampleTimestamp(i);
    int64_t next_timestamp = player_fixture.GetAudioSampleTimestamp(i + 1);
    int64_t buffer_duration = next_timestamp - current_timestamp;
    int64_t segment_duration = buffer_duration * kSegmentSize;
    int64_t written_duration = 0;
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

  int64_t start_system_time = starboard::CurrentMonotonicTime();
  int64_t start_media_time = player_fixture.GetCurrentMediaTime();

  ASSERT_NO_FATAL_FAILURE(player_fixture.WaitForPlayerEndOfStream());

  int64_t end_system_time = starboard::CurrentMonotonicTime();
  int64_t end_media_time = player_fixture.GetCurrentMediaTime();

  const int64_t kDurationDifferenceAllowance = 500'000;  // 500ms;
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

TEST_P(SbPlayerWriteSampleTest, DiscardAllAudio) {
  if (!IsPartialAudioSupported()) {
    GTEST_SKIP() << "The platform doesn't support partial audio.";
  }
  if (IsAudioPassthroughUsed(GetParam())) {
    GTEST_SKIP() << "The audio passthrough doesn't support partial audio.";
  }

  SbPlayerTestFixture player_fixture(GetParam(),
                                     &fake_graphics_context_provider_);
  if (HasFatalFailure()) {
    return;
  }
  if (!player_fixture.HasAudio()) {
    GTEST_SKIP() << "Skip PartialAudio test for audioless content.";
  }

  const int64_t kDurationToPlay = 1'000'000;  // 1 second
  const int64_t kDurationPerWrite = 100'000;  // 100ms
  const int64_t kNumberOfBuffersToDiscard = 20;

  GroupedSamples samples;
  if (player_fixture.HasVideo()) {
    samples.AddVideoSamples(
        0, player_fixture.ConvertDurationToVideoBufferCount(kDurationToPlay));
    samples.AddVideoEOS();
  }

  int written_buffer_index = 0;
  int num_of_buffers_per_write =
      player_fixture.ConvertDurationToAudioBufferCount(kDurationPerWrite);
  for (int count = 0; count < kDurationToPlay / kDurationPerWrite; ++count) {
    const int64_t kDurationToDiscard =
        count % 2 == 0 ? 1'000'000LL : std::numeric_limits<int64_t>::max();

    int64_t current_timestamp =
        player_fixture.GetAudioSampleTimestamp(written_buffer_index);
    // Discard from front.
    for (int i = 0; i < kNumberOfBuffersToDiscard; i++) {
      samples.AddAudioSamples(written_buffer_index, 1, current_timestamp,
                              kDurationToDiscard, 0);
    }

    // Discard from back.
    for (int i = 0; i < kNumberOfBuffersToDiscard; i++) {
      samples.AddAudioSamples(written_buffer_index, 1, current_timestamp, 0,
                              kDurationToDiscard);
    }

    samples.AddAudioSamples(written_buffer_index, num_of_buffers_per_write);
    written_buffer_index += num_of_buffers_per_write;
  }
  samples.AddAudioEOS();

  ASSERT_NO_FATAL_FAILURE(player_fixture.Write(samples));
  ASSERT_NO_FATAL_FAILURE(player_fixture.WaitForPlayerPresenting());

  int64_t start_system_time = starboard::CurrentMonotonicTime();
  int64_t start_media_time = player_fixture.GetCurrentMediaTime();

  ASSERT_NO_FATAL_FAILURE(player_fixture.WaitForPlayerEndOfStream());

  int64_t end_system_time = starboard::CurrentMonotonicTime();
  int64_t end_media_time = player_fixture.GetCurrentMediaTime();

  const int64_t kDurationDifferenceAllowance = 500'000;  // 500ms
  int64_t total_written_duration =
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

INSTANTIATE_TEST_SUITE_P(SbPlayerWriteSampleTests,
                         SbPlayerWriteSampleTest,
                         ValuesIn(GetAllPlayerTestConfigs()),
                         GetSbPlayerTestConfigName);

}  // namespace
}  // namespace nplb
