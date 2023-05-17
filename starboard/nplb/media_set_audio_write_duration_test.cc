// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/media.h"

#include "starboard/common/optional.h"
#include "starboard/common/spin_lock.h"
#include "starboard/configuration_constants.h"
#include "starboard/nplb/player_creation_param_helpers.h"
#include "starboard/nplb/player_test_util.h"
#include "starboard/player.h"
#include "starboard/shared/starboard/media/media_support_internal.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/video_dmp_reader.h"
#include "starboard/testing/fake_graphics_context_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

#if SB_API_VERSION < 15

namespace starboard {
namespace nplb {
namespace {

using ::shared::starboard::player::video_dmp::VideoDmpReader;
using ::starboard::testing::FakeGraphicsContextProvider;
using ::testing::ValuesIn;

const SbTime kDuration = kSbTimeSecond / 2;
const SbTime kSmallWaitInterval = 10 * kSbTimeMillisecond;

class SbMediaSetAudioWriteDurationTest
    : public ::testing::TestWithParam<const char*> {
 public:
  SbMediaSetAudioWriteDurationTest()
      : dmp_reader_(ResolveTestFileName(GetParam()).c_str()) {}

  void TryToWritePendingSample() {
    {
      starboard::ScopedSpinLock lock(&pending_decoder_status_lock_);
      if (!pending_decoder_status_.has_engaged()) {
        return;
      }
    }

    // If we have played the full duration required, then stop.
    if ((last_input_timestamp_ - first_input_timestamp_) >= total_duration_) {
      return;
    }

    // Check if we're about to input too far beyond the current playback time.
#if SB_API_VERSION >= 15
    SbPlayerInfo info;
    SbPlayerGetInfo(pending_decoder_status_->player, &info);
#else   // SB_API_VERSION >= 15
    SbPlayerInfo2 info;
    SbPlayerGetInfo2(pending_decoder_status_->player, &info);
#endif  // SB_API_VERSION >= 15
    if ((last_input_timestamp_ - info.current_media_timestamp) > kDuration) {
      // Postpone writing samples.
      return;
    }

    SbPlayer player = pending_decoder_status_->player;
    SbMediaType type = pending_decoder_status_->type;
    int ticket = pending_decoder_status_->ticket;
    {
      starboard::ScopedSpinLock lock(&pending_decoder_status_lock_);
      pending_decoder_status_ = nullopt;
    }

    CallSbPlayerWriteSamples(player, kSbMediaTypeAudio, dmp_reader_.get(),
                             index_, 1);
    ++index_;

    last_input_timestamp_ = player_sample_info.timestamp;
  }

  void PlayerStatus(SbPlayer player, SbPlayerState state, int ticket) {
    player_state_ = state;
  }

  void StorePendingDecoderStatus(SbPlayer player,
                                 SbMediaType type,
                                 int ticket) {
    starboard::ScopedSpinLock lock(&pending_decoder_status_lock_);
    SB_DCHECK(!pending_decoder_status_.has_engaged());
    PendingDecoderStatus pending_decoder_status = {};
    pending_decoder_status.player = player;
    pending_decoder_status.type = type;
    pending_decoder_status.ticket = ticket;
    pending_decoder_status_ = pending_decoder_status;
  }

  SbPlayer CreatePlayer() {
    SbMediaAudioSampleInfo audio_sample_info = dmp_reader_.audio_sample_info();
    SbMediaAudioCodec kAudioCodec = dmp_reader_.audio_codec();
    SbDrmSystem kDrmSystem = kSbDrmSystemInvalid;

    last_input_timestamp_ =
        dmp_reader_.GetPlayerSampleInfo(kSbMediaTypeAudio, 0).timestamp;
    first_input_timestamp_ = last_input_timestamp_;

    SbPlayerCreationParam creation_param = CreatePlayerCreationParam(
        audio_sample_info.codec, kSbMediaVideoCodecNone,
        SbPlayerGetPreferredOutputMode(&creation_param));
    creation_param.audio_sample_info = audio_sample_info;
    EXPECT_NE(creation_param.output_mode, kSbPlayerOutputModeInvalid);

    SbPlayer player = SbPlayerCreate(
        fake_graphics_context_provider_.window(), &creation_param,
        DummyDeallocateSampleFunc, DecoderStatusFunc, PlayerStatusFunc,
        DummyErrorFunc, this /* context */,
        fake_graphics_context_provider_.decoder_target_provider());

    EXPECT_TRUE(SbPlayerIsValid(player));
    return player;
  }

  void WaitForPlayerState(SbPlayerState desired_state) {
    SbTime start_of_wait = SbTimeGetMonotonicNow();
    const SbTime kMaxWaitTime = 3 * kSbTimeSecond;
    while (player_state_ != desired_state &&
           (SbTimeGetMonotonicNow() - start_of_wait) < kMaxWaitTime) {
      SbThreadSleep(kSmallWaitInterval);
      TryToWritePendingSample();
    }
    ASSERT_EQ(desired_state, player_state_);
  }

 protected:
  struct PendingDecoderStatus {
    SbPlayer player;
    SbMediaType type;
    int ticket;
  };

  FakeGraphicsContextProvider fake_graphics_context_provider_;
  VideoDmpReader dmp_reader_;
  SbPlayerState player_state_ = kSbPlayerStateInitialized;
  SbTime last_input_timestamp_ = 0;
  SbTime first_input_timestamp_ = 0;
  int index_ = 0;
  SbTime total_duration_ = kDuration;
  // Guard access to |pending_decoder_status_|.
  mutable SbAtomic32 pending_decoder_status_lock_ =
      starboard::kSpinLockStateReleased;
  optional<PendingDecoderStatus> pending_decoder_status_;

 private:
  static void DecoderStatusFunc(SbPlayer player,
                                void* context,
                                SbMediaType type,
                                SbPlayerDecoderState state,
                                int ticket) {
    if (state != kSbPlayerDecoderStateNeedsData) {
      return;
    }
    static_cast<SbMediaSetAudioWriteDurationTest*>(context)
        ->StorePendingDecoderStatus(player, type, ticket);
  }

  static void PlayerStatusFunc(SbPlayer player,
                               void* context,
                               SbPlayerState state,
                               int ticket) {
    static_cast<SbMediaSetAudioWriteDurationTest*>(context)->PlayerStatus(
        player, state, ticket);
  }
};

TEST_P(SbMediaSetAudioWriteDurationTest, WriteLimitedInput) {
  ASSERT_NE(dmp_reader_.audio_codec(), kSbMediaAudioCodecNone);
  ASSERT_GT(dmp_reader_.number_of_audio_buffers(), 0);

  SbMediaSetAudioWriteDuration(kDuration);

  SbPlayer player = CreatePlayer();
  WaitForPlayerState(kSbPlayerStateInitialized);

  // Seek to preroll.
#if SB_API_VERSION >= 15
  SbPlayerSeek(player, first_input_timestamp_, /* ticket */ 1);
#else   // SB_API_VERSION >= 15
  SbPlayerSeek2(player, first_input_timestamp_, /* ticket */ 1);
#endif  // SB_API_VERSION >= 15

  WaitForPlayerState(kSbPlayerStatePresenting);

  // Wait until the playback time is > 0.
  const SbTime kMaxWaitTime = 5 * kSbTimeSecond;
  SbTime start_of_wait = SbTimeGetMonotonicNow();
#if SB_API_VERSION >= 15
  SbPlayerInfo info = {};
#else   // SB_API_VERSION >= 15
  SbPlayerInfo2 info = {};
#endif  // SB_API_VERSION >= 15
  while (SbTimeGetMonotonicNow() - start_of_wait < kMaxWaitTime &&
         info.current_media_timestamp == 0) {
    SbThreadSleep(kSbTimeMillisecond * 500);
#if SB_API_VERSION >= 15
    SbPlayerGetInfo(player, &info);
#else   // SB_API_VERSION >= 15
    SbPlayerGetInfo2(player, &info);
#endif  // SB_API_VERSION >= 15
  }

  EXPECT_GT(info.current_media_timestamp, 0);

  SbPlayerDestroy(player);
}

TEST_P(SbMediaSetAudioWriteDurationTest, WriteContinuedLimitedInput) {
  ASSERT_NE(dmp_reader_.audio_codec(), kSbMediaAudioCodecNone);
  ASSERT_GT(dmp_reader_.number_of_audio_buffers(), 0);

  SbMediaSetAudioWriteDuration(kDuration);

  // This directly impacts the runtime of the test.
  total_duration_ = 15 * kSbTimeSecond;

  SbPlayer player = CreatePlayer();
  WaitForPlayerState(kSbPlayerStateInitialized);

  // Seek to preroll.
#if SB_API_VERSION >= 15
  SbPlayerSeek(player, first_input_timestamp_, /* ticket */ 1);
#else   // SB_API_VERSION >= 15
  SbPlayerSeek2(player, first_input_timestamp_, /* ticket */ 1);
#endif  // SB_API_VERSION >= 15
  WaitForPlayerState(kSbPlayerStatePresenting);

  // Wait for the player to play far enough. It may not play all the way to
  // the end, but it should leave off no more than |kDuration|.
  SbTime min_ending_playback_time = total_duration_ - kDuration;
  SbTime start_of_wait = SbTimeGetMonotonicNow();
  const SbTime kMaxWaitTime = total_duration_ + 5 * kSbTimeSecond;
#if SB_API_VERSION >= 15
  SbPlayerInfo info;
  SbPlayerGetInfo(player, &info);
#else   // SB_API_VERSION >= 15
  SbPlayerInfo2 info;
  SbPlayerGetInfo2(player, &info);
#endif  // SB_API_VERSION >= 15
  while (info.current_media_timestamp < min_ending_playback_time &&
         (SbTimeGetMonotonicNow() - start_of_wait) < kMaxWaitTime) {
#if SB_API_VERSION >= 15
    SbPlayerGetInfo(player, &info);
#else   // SB_API_VERSION >= 15
    SbPlayerGetInfo2(player, &info);
#endif  // SB_API_VERSION >= 15
    SbThreadSleep(kSmallWaitInterval);
    TryToWritePendingSample();
  }
  EXPECT_GE(info.current_media_timestamp, min_ending_playback_time);
  SbPlayerDestroy(player);
}

std::vector<const char*> GetSupportedTests() {
  const char* kFilenames[] = {"beneath_the_canopy_aac_stereo.dmp",
                              "beneath_the_canopy_opus_stereo.dmp"};

  static std::vector<const char*> test_params;

  if (!test_params.empty()) {
    return test_params;
  }

  for (auto filename : kFilenames) {
    VideoDmpReader dmp_reader(ResolveTestFileName(filename).c_str());
    SB_DCHECK(dmp_reader.number_of_audio_buffers() > 0);

    const SbMediaAudioSampleInfo* audio_sample_info =
        &dmp_reader.audio_sample_info();
    if (SbMediaIsAudioSupported(dmp_reader.audio_codec(), nullptr,
                                dmp_reader.audio_bitrate())) {
      test_params.push_back(filename);
    }
  }
  SB_DCHECK(!test_params.empty());
  return test_params;
}

INSTANTIATE_TEST_CASE_P(SbMediaSetAudioWriteDurationTests,
                        SbMediaSetAudioWriteDurationTest,
                        ValuesIn(GetSupportedTests()));
}  // namespace
}  // namespace nplb
}  // namespace starboard

#endif  // SB_API_VERSION < 15
