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

#include <unistd.h>

#include <atomic>
#include <optional>

#include "starboard/common/spin_lock.h"
#include "starboard/common/time.h"
#include "starboard/configuration_constants.h"
#include "starboard/media.h"
#include "starboard/nplb/player_creation_param_helpers.h"
#include "starboard/nplb/player_test_util.h"
#include "starboard/player.h"
#include "starboard/shared/starboard/media/media_support_internal.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/video_dmp_reader.h"
#include "starboard/testing/fake_graphics_context_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

using ::starboard::FakeGraphicsContextProvider;
using ::starboard::VideoDmpReader;
using ::testing::ValuesIn;

const int64_t kDuration = 500'000;          // 0.5 seconds
const int64_t kSmallWaitInterval = 10'000;  // 10 ms

class SbMediaSetAudioWriteDurationTest
    : public ::testing::TestWithParam<const char*> {
 public:
  SbMediaSetAudioWriteDurationTest() : dmp_reader_(GetParam()) {}

  void SetUp() override {
    SbMediaAudioCodec audio_codec = dmp_reader_.audio_codec();
    PlayerCreationParam creation_param = CreatePlayerCreationParam(
        audio_codec, kSbMediaVideoCodecNone, kSbPlayerOutputModeInvalid);
    SbPlayerCreationParam param = {};
    creation_param.ConvertTo(&param);
    creation_param.output_mode = SbPlayerGetPreferredOutputMode(&param);

    SbPlayerTestConfig test_config(GetParam(), "", creation_param.output_mode,
                                   "");
    SkipTestIfNotSupported(test_config);
  }

  void TryToWritePendingSample() {
    {
      starboard::ScopedSpinLock lock(&pending_decoder_status_lock_);
      if (!pending_decoder_status_.has_value()) {
        return;
      }
    }

    // If we have played the full duration required, then stop.
    if ((last_input_timestamp_ - first_input_timestamp_) >= total_duration_) {
      return;
    }

    // Check if we're about to input too far beyond the current playback time.
    SbPlayerInfo info;
    SbPlayerGetInfo(pending_decoder_status_->player, &info);
    if ((last_input_timestamp_ - info.current_media_timestamp) > kDuration) {
      // Postpone writing samples.
      return;
    }

    SbPlayer player = pending_decoder_status_->player;
    {
      starboard::ScopedSpinLock lock(&pending_decoder_status_lock_);
      pending_decoder_status_ = std::nullopt;
    }

    CallSbPlayerWriteSamples(player, kSbMediaTypeAudio, &dmp_reader_, index_,
                             1);
    last_input_timestamp_ =
        dmp_reader_.GetPlayerSampleInfo(kSbMediaTypeAudio, index_).timestamp;
    ++index_;
  }

  void PlayerStatus(SbPlayer player, SbPlayerState state, int ticket) {
    player_state_ = state;
  }

  void StorePendingDecoderStatus(SbPlayer player,
                                 SbMediaType type,
                                 int ticket) {
    starboard::ScopedSpinLock lock(&pending_decoder_status_lock_);
    SB_DCHECK(!pending_decoder_status_.has_value());
    PendingDecoderStatus pending_decoder_status = {};
    pending_decoder_status.player = player;
    pending_decoder_status.type = type;
    pending_decoder_status.ticket = ticket;
    pending_decoder_status_ = pending_decoder_status;
  }

  SbPlayer CreatePlayer() {
    SbMediaAudioCodec audio_codec = dmp_reader_.audio_codec();

    PlayerCreationParam creation_param = CreatePlayerCreationParam(
        audio_codec, kSbMediaVideoCodecNone, kSbPlayerOutputModeInvalid);

    SbPlayerCreationParam param = {};
    creation_param.ConvertTo(&param);

    creation_param.output_mode = SbPlayerGetPreferredOutputMode(&param);
    EXPECT_NE(creation_param.output_mode, kSbPlayerOutputModeInvalid);

    SbPlayer player = CallSbPlayerCreate(
        fake_graphics_context_provider_.window(), kSbMediaVideoCodecNone,
        audio_codec, kSbDrmSystemInvalid, &dmp_reader_.audio_stream_info(), "",
        DummyDeallocateSampleFunc, DecoderStatusFunc, PlayerStatusFunc,
        DummyErrorFunc, this /* context */, creation_param.output_mode,
        fake_graphics_context_provider_.decoder_target_provider());

    EXPECT_TRUE(SbPlayerIsValid(player));

    last_input_timestamp_ =
        dmp_reader_.GetPlayerSampleInfo(kSbMediaTypeAudio, 0).timestamp;
    first_input_timestamp_ = last_input_timestamp_;

    return player;
  }

  void WaitForPlayerState(SbPlayerState desired_state) {
    int64_t start_of_wait = starboard::CurrentMonotonicTime();
    const int64_t kMaxWaitTime = 3'000'000LL;  // 3 seconds
    while (player_state_ != desired_state &&
           (starboard::CurrentMonotonicTime() - start_of_wait) < kMaxWaitTime) {
      usleep(kSmallWaitInterval);
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
  int64_t last_input_timestamp_ = 0;
  int64_t first_input_timestamp_ = 0;
  int index_ = 0;
  int64_t total_duration_ = kDuration;
  // Guard access to |pending_decoder_status_|.
  mutable std::atomic_int pending_decoder_status_lock_{
      starboard::kSpinLockStateReleased};
  std::optional<PendingDecoderStatus> pending_decoder_status_;

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
  ASSERT_GT(dmp_reader_.number_of_audio_buffers(), 0u);

  SbPlayer player = CreatePlayer();
  WaitForPlayerState(kSbPlayerStateInitialized);

  // Seek to preroll.
  SbPlayerSeek(player, first_input_timestamp_, /* ticket */ 1);

  WaitForPlayerState(kSbPlayerStatePresenting);

  // Wait until the playback time is > 0.
  const int64_t kMaxWaitTime = 5'000'000;  // 5 seconds
  int64_t start_of_wait = starboard::CurrentMonotonicTime();
  SbPlayerInfo info = {};
  while (starboard::CurrentMonotonicTime() - start_of_wait < kMaxWaitTime &&
         info.current_media_timestamp == 0) {
    usleep(500'000);
    SbPlayerGetInfo(player, &info);
  }

  EXPECT_GT(info.current_media_timestamp, 0);

  SbPlayerDestroy(player);
}

TEST_P(SbMediaSetAudioWriteDurationTest, WriteContinuedLimitedInput) {
  ASSERT_NE(dmp_reader_.audio_codec(), kSbMediaAudioCodecNone);
  ASSERT_GT(dmp_reader_.number_of_audio_buffers(), 0u);

  // This directly impacts the runtime of the test.
  total_duration_ = 15'000'000LL;  // 15 seconds

  SbPlayer player = CreatePlayer();
  WaitForPlayerState(kSbPlayerStateInitialized);

  // Seek to preroll.
  SbPlayerSeek(player, first_input_timestamp_, /* ticket */ 1);
  WaitForPlayerState(kSbPlayerStatePresenting);

  // Wait for the player to play far enough. It may not play all the way to
  // the end, but it should leave off no more than |kDuration|.
  int64_t min_ending_playback_time = total_duration_ - kDuration;
  int64_t start_of_wait = starboard::CurrentMonotonicTime();
  const int64_t kMaxWaitTime = total_duration_ + 5'000'000LL;
  SbPlayerInfo info;
  SbPlayerGetInfo(player, &info);
  while (info.current_media_timestamp < min_ending_playback_time &&
         (starboard::CurrentMonotonicTime() - start_of_wait) < kMaxWaitTime) {
    SbPlayerGetInfo(player, &info);
    usleep(kSmallWaitInterval);
    TryToWritePendingSample();
  }
  EXPECT_GE(info.current_media_timestamp, min_ending_playback_time);
  SbPlayerDestroy(player);
}

INSTANTIATE_TEST_SUITE_P(SbMediaSetAudioWriteDurationTests,
                         SbMediaSetAudioWriteDurationTest,
                         ValuesIn(GetStereoAudioTestFiles()));
}  // namespace
}  // namespace nplb
