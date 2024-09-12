// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/nplb/player_test_fixture.h"
#include "starboard/testing/fake_graphics_context_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

using ::testing::ValuesIn;

typedef SbPlayerTestFixture::GroupedSamples GroupedSamples;
typedef testing::FakeGraphicsContextProvider FakeGraphicsContextProvider;

class SbPlayerGetMediaTimeTest
    : public ::testing::TestWithParam<SbPlayerTestConfig> {
 protected:
  FakeGraphicsContextProvider fake_graphics_context_provider_;
};

TEST_P(SbPlayerGetMediaTimeTest, SunnyDay) {
  const int64_t kDurationToPlay = 1'000'000;  // 1 second

  SbPlayerTestFixture player_fixture(GetParam(),
                                     &fake_graphics_context_provider_);
  if (HasFatalFailure()) {
    return;
  }

  int64_t media_time_before_write = player_fixture.GetCurrentMediaTime();
  ASSERT_EQ(media_time_before_write, 0);

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
  ASSERT_NO_FATAL_FAILURE(player_fixture.WaitForPlayerPresenting());

  int64_t start_system_time = CurrentMonotonicTime();
  int64_t start_media_time = player_fixture.GetCurrentMediaTime();

  ASSERT_NO_FATAL_FAILURE(player_fixture.WaitForPlayerEndOfStream());

  int64_t end_system_time = CurrentMonotonicTime();
  int64_t end_media_time = player_fixture.GetCurrentMediaTime();

  const int64_t kDurationDifferenceAllowance = 500'000;  // 500 ms
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

TEST_P(SbPlayerGetMediaTimeTest, TimeAfterSeek) {
  const int kSamplesToWrite = 8;

  SbPlayerTestFixture player_fixture(GetParam(),
                                     &fake_graphics_context_provider_);
  if (HasFatalFailure()) {
    return;
  }

  GroupedSamples samples;
  if (player_fixture.HasAudio()) {
    samples.AddAudioSamples(0, kSamplesToWrite);
    samples.AddAudioEOS();
  }
  if (player_fixture.HasVideo()) {
    samples.AddVideoSamples(0, kSamplesToWrite);
    samples.AddVideoEOS();
  }
  ASSERT_NO_FATAL_FAILURE(player_fixture.Write(samples));
  ASSERT_NO_FATAL_FAILURE(player_fixture.WaitForPlayerPresenting());

  const int64_t seek_to_time = 1'000'000;  // 1 second
  ASSERT_NO_FATAL_FAILURE(player_fixture.Seek(seek_to_time));

  const int64_t kDurationToPlay = 1'000'000;  // 1 second
  samples = GroupedSamples();
  if (player_fixture.HasAudio()) {
    samples.AddAudioSamples(
        0,
        player_fixture.ConvertDurationToAudioBufferCount(seek_to_time) +
            player_fixture.ConvertDurationToAudioBufferCount(kDurationToPlay));
    samples.AddAudioEOS();
  }
  if (player_fixture.HasVideo()) {
    samples.AddVideoSamples(
        0,
        player_fixture.ConvertDurationToVideoBufferCount(seek_to_time) +
            player_fixture.ConvertDurationToVideoBufferCount(kDurationToPlay));
    samples.AddVideoEOS();
  }
  ASSERT_NO_FATAL_FAILURE(player_fixture.Write(samples));
  ASSERT_NO_FATAL_FAILURE(player_fixture.WaitForPlayerPresenting());

  int64_t start_system_time = CurrentMonotonicTime();
  int64_t start_media_time = player_fixture.GetCurrentMediaTime();

  ASSERT_NO_FATAL_FAILURE(player_fixture.WaitForPlayerEndOfStream());

  int64_t end_system_time = CurrentMonotonicTime();
  int64_t end_media_time = player_fixture.GetCurrentMediaTime();

  const int64_t kDurationDifferenceAllowance = 500'000;  // 500 ms
  EXPECT_NEAR(end_media_time, kDurationToPlay + seek_to_time,
              kDurationDifferenceAllowance);
  EXPECT_NEAR(
      end_system_time - start_system_time + start_media_time - seek_to_time,
      kDurationToPlay, kDurationDifferenceAllowance);

  SB_DLOG(INFO) << "The expected media time should be "
                << kDurationToPlay + seek_to_time
                << ", the actual media time is " << end_media_time
                << ", with difference "
                << std::abs(end_media_time - kDurationToPlay - seek_to_time)
                << ".";
  SB_DLOG(INFO) << "The expected total playing time should be "
                << kDurationToPlay << ", the actual playing time is "
                << end_system_time - start_system_time + start_media_time -
                       seek_to_time
                << ", with difference "
                << std::abs(end_system_time - start_system_time +
                            start_media_time - seek_to_time - kDurationToPlay)
                << ".";
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

INSTANTIATE_TEST_CASE_P(SbPlayerGetMediaTimeTests,
                        SbPlayerGetMediaTimeTest,
                        ValuesIn(GetSupportedTestConfigs()),
                        GetSbPlayerTestConfigName);

}  // namespace
}  // namespace nplb
}  // namespace starboard
