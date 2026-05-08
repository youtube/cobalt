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

#include "starboard/common/time.h"
#include "starboard/nplb/player_test_fixture.h"
#include "starboard/shared/starboard/media/media_time_delta.h"
#include "starboard/testing/fake_graphics_context_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

using ::starboard::FakeGraphicsContextProvider;
using ::starboard::MediaTimeDelta;
using ::testing::ValuesIn;

typedef SbPlayerTestFixture::GroupedSamples GroupedSamples;

class SbPlayerGetMediaTimeTest
    : public ::testing::TestWithParam<SbPlayerTestConfig> {
 protected:
  void SetUp() override { SkipTestIfNotSupported(GetParam()); }

  FakeGraphicsContextProvider fake_graphics_context_provider_;
};

TEST_P(SbPlayerGetMediaTimeTest, SunnyDay) {
  const MediaTimeDelta kDurationToPlay = MediaTimeDelta::FromSeconds(1);

  SbPlayerTestFixture player_fixture(GetParam(),
                                     &fake_graphics_context_provider_);
  if (HasFatalFailure()) {
    return;
  }

  int64_t media_time_before_write = player_fixture.GetCurrentMediaTime();
  ASSERT_EQ(media_time_before_write, 0);

  GroupedSamples samples;
  if (player_fixture.HasAudio()) {
    samples.AddAudioSamples(0, player_fixture.ConvertDurationToAudioBufferCount(
                                   kDurationToPlay.InMicroseconds()));
    samples.AddAudioEOS();
  }
  if (player_fixture.HasVideo()) {
    samples.AddVideoSamples(0, player_fixture.ConvertDurationToVideoBufferCount(
                                   kDurationToPlay.InMicroseconds()));
    samples.AddVideoEOS();
  }
  ASSERT_NO_FATAL_FAILURE(player_fixture.Write(samples));
  ASSERT_NO_FATAL_FAILURE(player_fixture.WaitForPlayerPresenting());

  int64_t start_system_time = starboard::CurrentMonotonicTime();
  int64_t start_media_time = player_fixture.GetCurrentMediaTime();

  ASSERT_NO_FATAL_FAILURE(player_fixture.WaitForPlayerEndOfStream());

  int64_t end_system_time = starboard::CurrentMonotonicTime();
  int64_t end_media_time = player_fixture.GetCurrentMediaTime();

  const MediaTimeDelta kDurationDifferenceAllowance =
      MediaTimeDelta::FromMilliseconds(500);
  EXPECT_NEAR(end_media_time, kDurationToPlay.InMicroseconds(),
              kDurationDifferenceAllowance.InMicroseconds());
  EXPECT_NEAR(end_system_time - start_system_time + start_media_time,
              kDurationToPlay.InMicroseconds(),
              kDurationDifferenceAllowance.InMicroseconds());

  SB_DLOG(INFO) << "The expected media time should be "
                << kDurationToPlay.InMicroseconds()
                << ", the actual media time is " << end_media_time
                << ", with difference "
                << std::abs(end_media_time - kDurationToPlay.InMicroseconds())
                << ".";
  SB_DLOG(INFO) << "The expected total playing time should be "
                << kDurationToPlay.InMicroseconds()
                << ", the actual playing time is "
                << end_system_time - start_system_time + start_media_time
                << ", with difference "
                << std::abs(end_system_time - start_system_time +
                            start_media_time - kDurationToPlay.InMicroseconds())
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

  const MediaTimeDelta seek_to_time = MediaTimeDelta::FromSeconds(1);
  ASSERT_NO_FATAL_FAILURE(player_fixture.Seek(seek_to_time.InMicroseconds()));

  const MediaTimeDelta kDurationToPlay = MediaTimeDelta::FromSeconds(1);
  samples = GroupedSamples();
  if (player_fixture.HasAudio()) {
    samples.AddAudioSamples(
        0, player_fixture.ConvertDurationToAudioBufferCount(
               seek_to_time.InMicroseconds()) +
               player_fixture.ConvertDurationToAudioBufferCount(
                   kDurationToPlay.InMicroseconds()));
    samples.AddAudioEOS();
  }
  if (player_fixture.HasVideo()) {
    samples.AddVideoSamples(
        0, player_fixture.ConvertDurationToVideoBufferCount(
               seek_to_time.InMicroseconds()) +
               player_fixture.ConvertDurationToVideoBufferCount(
                   kDurationToPlay.InMicroseconds()));
    samples.AddVideoEOS();
  }
  ASSERT_NO_FATAL_FAILURE(player_fixture.Write(samples));
  ASSERT_NO_FATAL_FAILURE(player_fixture.WaitForPlayerPresenting());

  int64_t start_system_time = starboard::CurrentMonotonicTime();
  int64_t start_media_time = player_fixture.GetCurrentMediaTime();

  ASSERT_NO_FATAL_FAILURE(player_fixture.WaitForPlayerEndOfStream());

  int64_t end_system_time = starboard::CurrentMonotonicTime();
  int64_t end_media_time = player_fixture.GetCurrentMediaTime();

  const MediaTimeDelta kDurationDifferenceAllowance =
      MediaTimeDelta::FromMilliseconds(500);
  EXPECT_NEAR(end_media_time, (kDurationToPlay + seek_to_time).InMicroseconds(),
              kDurationDifferenceAllowance.InMicroseconds());
  EXPECT_NEAR(end_system_time - start_system_time + start_media_time -
                  seek_to_time.InMicroseconds(),
              kDurationToPlay.InMicroseconds(),
              kDurationDifferenceAllowance.InMicroseconds());

  SB_DLOG(INFO) << "The expected media time should be "
                << (kDurationToPlay + seek_to_time).InMicroseconds()
                << ", the actual media time is " << end_media_time
                << ", with difference "
                << std::abs(end_media_time -
                            (kDurationToPlay + seek_to_time).InMicroseconds())
                << ".";
  SB_DLOG(INFO) << "The expected total playing time should be "
                << kDurationToPlay.InMicroseconds()
                << ", the actual playing time is "
                << end_system_time - start_system_time + start_media_time -
                       seek_to_time.InMicroseconds()
                << ", with difference "
                << std::abs(end_system_time - start_system_time +
                            start_media_time - seek_to_time.InMicroseconds() -
                            kDurationToPlay.InMicroseconds())
                << ".";
}

INSTANTIATE_TEST_SUITE_P(SbPlayerGetMediaTimeTests,
                         SbPlayerGetMediaTimeTest,
                         ValuesIn(GetAllPlayerTestConfigs()),
                         GetSbPlayerTestConfigName);

}  // namespace
}  // namespace nplb
