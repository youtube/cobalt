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

#include "starboard/common/duration.h"
#include "starboard/common/time.h"
#include "starboard/nplb/player_test_fixture.h"
#include "starboard/testing/fake_graphics_context_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

using ::starboard::FakeGraphicsContextProvider;
using ::testing::ValuesIn;

typedef SbPlayerTestFixture::GroupedSamples GroupedSamples;

class SbPlayerGetMediaTimeTest
    : public ::testing::TestWithParam<SbPlayerTestConfig> {
 protected:
  void SetUp() override { SkipTestIfNotSupported(GetParam()); }

  FakeGraphicsContextProvider fake_graphics_context_provider_;
};

TEST_P(SbPlayerGetMediaTimeTest, SunnyDay) {
  const auto kDurationToPlay = 1s;

  SbPlayerTestFixture player_fixture(GetParam(),
                                     &fake_graphics_context_provider_);
  if (HasFatalFailure()) {
    return;
  }

  microseconds media_time_before_write = player_fixture.GetCurrentMediaTime();
  ASSERT_EQ(media_time_before_write, microseconds::zero());

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

  int64_t start_system_time = starboard::CurrentMonotonicTime();
  microseconds start_media_time = player_fixture.GetCurrentMediaTime();

  ASSERT_NO_FATAL_FAILURE(player_fixture.WaitForPlayerEndOfStream());

  int64_t end_system_time = starboard::CurrentMonotonicTime();
  microseconds end_media_time = player_fixture.GetCurrentMediaTime();

  const auto kDurationDifferenceAllowance = 500ms;
  EXPECT_NEAR(end_media_time.count(), microseconds(kDurationToPlay).count(),
              microseconds(kDurationDifferenceAllowance).count());
  microseconds system_time_diff(end_system_time - start_system_time);
  EXPECT_NEAR((system_time_diff + start_media_time).count(),
              microseconds(kDurationToPlay).count(),
              microseconds(kDurationDifferenceAllowance).count());

  SB_DLOG(INFO) << "The expected media time should be "
                << microseconds(kDurationToPlay).count()
                << " usec, the actual media time is " << end_media_time.count()
                << " usec, with difference "
                << std::abs((end_media_time - kDurationToPlay).count())
                << " usec.";
  SB_DLOG(INFO)
      << "The expected total playing time should be "
      << microseconds(kDurationToPlay).count()
      << " usec, the actual playing time is "
      << (system_time_diff + start_media_time).count()
      << " usec, with difference "
      << std::abs(
             (system_time_diff + start_media_time - kDurationToPlay).count())
      << " usec.";
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

  const auto seek_to_time = 1s;
  ASSERT_NO_FATAL_FAILURE(player_fixture.Seek(seek_to_time));

  const auto kDurationToPlay = 1s;
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

  int64_t start_system_time = starboard::CurrentMonotonicTime();
  microseconds start_media_time = player_fixture.GetCurrentMediaTime();

  ASSERT_NO_FATAL_FAILURE(player_fixture.WaitForPlayerEndOfStream());

  int64_t end_system_time = starboard::CurrentMonotonicTime();
  microseconds end_media_time = player_fixture.GetCurrentMediaTime();

  const auto kDurationDifferenceAllowance = 500ms;
  EXPECT_NEAR(end_media_time.count(),
              microseconds(kDurationToPlay + seek_to_time).count(),
              microseconds(kDurationDifferenceAllowance).count());
  microseconds system_time_diff(end_system_time - start_system_time);
  EXPECT_NEAR((system_time_diff + start_media_time - seek_to_time).count(),
              microseconds(kDurationToPlay).count(),
              microseconds(kDurationDifferenceAllowance).count());

  SB_DLOG(INFO)
      << "The expected media time should be "
      << microseconds(kDurationToPlay + seek_to_time).count()
      << " usec, the actual media time is " << end_media_time.count()
      << " usec, with difference "
      << std::abs((end_media_time - kDurationToPlay - seek_to_time).count())
      << " usec.";
  SB_DLOG(INFO) << "The expected total playing time should be "
                << microseconds(kDurationToPlay).count()
                << " usec, the actual playing time is "
                << (system_time_diff + start_media_time - seek_to_time).count()
                << " usec, with difference "
                << std::abs((system_time_diff + start_media_time -
                             seek_to_time - kDurationToPlay)
                                .count())
                << " usec.";
}

INSTANTIATE_TEST_SUITE_P(SbPlayerGetMediaTimeTests,
                         SbPlayerGetMediaTimeTest,
                         ValuesIn(GetAllPlayerTestConfigs()),
                         GetSbPlayerTestConfigName);

}  // namespace
}  // namespace nplb
