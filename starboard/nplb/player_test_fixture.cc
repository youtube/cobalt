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

#include "starboard/nplb/player_test_fixture.h"

#include <algorithm>

#include "starboard/common/string.h"
#include "starboard/nplb/drm_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {

using shared::starboard::player::video_dmp::VideoDmpReader;
using testing::FakeGraphicsContextProvider;

SbPlayerTestFixture::CallbackEvent::CallbackEvent() : event_type(kEmptyEvent) {}

SbPlayerTestFixture::CallbackEvent::CallbackEvent(SbPlayer player,
                                                  SbMediaType type,
                                                  SbPlayerDecoderState state,
                                                  int ticket)
    : event_type(kDecoderStateEvent),
      player(player),
      media_type(type),
      decoder_state(state),
      ticket(ticket) {}

SbPlayerTestFixture::CallbackEvent::CallbackEvent(SbPlayer player,
                                                  SbPlayerState state,
                                                  int ticket)
    : event_type(kPlayerStateEvent),
      player(player),
      player_state(state),
      ticket(ticket) {}

SbPlayerTestFixture::SbPlayerTestFixture(const SbPlayerTestConfig& config)
    : output_mode_(std::get<2>(config)), key_system_(std::get<3>(config)) {
  SB_DCHECK(output_mode_ == kSbPlayerOutputModeDecodeToTexture ||
            output_mode_ == kSbPlayerOutputModePunchOut);

  const char* audio_dmp_filename = std::get<0>(config);
  const char* video_dmp_filename = std::get<1>(config);

  if (audio_dmp_filename && strlen(audio_dmp_filename) > 0) {
    audio_dmp_reader_.reset(new VideoDmpReader(
        audio_dmp_filename, VideoDmpReader::kEnableReadOnDemand));
  }
  if (video_dmp_filename && strlen(video_dmp_filename) > 0) {
    video_dmp_reader_.reset(new VideoDmpReader(
        video_dmp_filename, VideoDmpReader::kEnableReadOnDemand));
  }

  Initialize();
}

SbPlayerTestFixture::~SbPlayerTestFixture() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  TearDown();
}

void SbPlayerTestFixture::Seek(const SbTime time) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK(SbPlayerIsValid(player_));

  ASSERT_FALSE(error_occurred_);
  ASSERT_FALSE(HasReceivedPlayerState(kSbPlayerStateDestroyed));
  ASSERT_TRUE(HasReceivedPlayerState(kSbPlayerStateInitialized));

  can_accept_more_audio_data_ = false;
  can_accept_more_video_data_ = false;
  player_state_set_.clear();
  player_state_set_.insert(kSbPlayerStateInitialized);
  audio_end_of_stream_written_ = false;
  video_end_of_stream_written_ = false;

#if SB_API_VERSION >= 15
  SbPlayerSeek(player_, time, ++ticket_);
#else   // SB_API_VERSION >= 15
  SbPlayerSeek2(player_, time, ++ticket_);
#endif  // SB_API_VERSION >= 15
}

void SbPlayerTestFixture::Write(const GroupedSamples& grouped_samples) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK(SbPlayerIsValid(player_));
  SB_DCHECK(!audio_end_of_stream_written_);
  SB_DCHECK(!video_end_of_stream_written_);

  ASSERT_FALSE(error_occurred_);

  int audio_start_index = grouped_samples.audio_start_index();
  int audio_samples_to_write = grouped_samples.audio_samples_to_write();
  int video_start_index = grouped_samples.video_start_index();
  int video_samples_to_write = grouped_samples.video_samples_to_write();
  bool write_audio_eos = grouped_samples.write_audio_eos();
  bool write_video_eos = grouped_samples.write_video_eos();

  SB_DCHECK(audio_start_index >= 0);
  SB_DCHECK(audio_samples_to_write >= 0);
  SB_DCHECK(video_start_index >= 0);
  SB_DCHECK(video_samples_to_write >= 0);
  if (audio_samples_to_write > 0 || write_audio_eos) {
    SB_DCHECK(audio_dmp_reader_);
  }
  if (video_samples_to_write > 0 || write_video_eos) {
    SB_DCHECK(video_dmp_reader_);
  }

  int max_audio_samples_per_write =
      SbPlayerGetMaximumNumberOfSamplesPerWrite(player_, kSbMediaTypeAudio);
  int max_video_samples_per_write =
      SbPlayerGetMaximumNumberOfSamplesPerWrite(player_, kSbMediaTypeVideo);

  // Cap the samples to write to the end of the dmp files.
  if (audio_samples_to_write > 0) {
    audio_samples_to_write = std::min<int>(
        audio_samples_to_write,
        audio_dmp_reader_->number_of_audio_buffers() - audio_start_index);
  }
  if (video_samples_to_write > 0) {
    video_samples_to_write = std::min<int>(
        video_samples_to_write,
        video_dmp_reader_->number_of_video_buffers() - video_start_index);
  }

  bool has_more_audio = audio_samples_to_write > 0 || write_audio_eos;
  bool has_more_video = video_samples_to_write > 0 || write_video_eos;
  while (has_more_audio || has_more_video) {
    ASSERT_NO_FATAL_FAILURE(WaitForDecoderStateNeedsData());
    if (can_accept_more_audio_data_ && has_more_audio) {
      if (audio_samples_to_write > 0) {
        auto samples_to_write =
            std::min(max_audio_samples_per_write, audio_samples_to_write);
        ASSERT_NO_FATAL_FAILURE(WriteSamples(
            kSbMediaTypeAudio, audio_start_index, samples_to_write));
        audio_start_index += samples_to_write;
        audio_samples_to_write -= samples_to_write;
      } else if (!audio_end_of_stream_written_ && write_audio_eos) {
        ASSERT_NO_FATAL_FAILURE(WriteEndOfStream(kSbMediaTypeAudio));
      }
      has_more_audio = audio_samples_to_write > 0 ||
                       (!audio_end_of_stream_written_ && write_audio_eos);
    }

    if (can_accept_more_video_data_ && has_more_video) {
      if (video_samples_to_write > 0) {
        auto samples_to_write =
            std::min(max_video_samples_per_write, video_samples_to_write);
        ASSERT_NO_FATAL_FAILURE(WriteSamples(
            kSbMediaTypeVideo, video_start_index, samples_to_write));
        video_start_index += samples_to_write;
        video_samples_to_write -= samples_to_write;
      } else if (!video_end_of_stream_written_ && write_video_eos) {
        ASSERT_NO_FATAL_FAILURE(WriteEndOfStream(kSbMediaTypeVideo));
      }
      has_more_video = video_samples_to_write > 0 ||
                       (!video_end_of_stream_written_ && write_video_eos);
    }
  }
}

void SbPlayerTestFixture::WaitForPlayerEndOfStream() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK(SbPlayerIsValid(player_));
  SB_DCHECK(!audio_dmp_reader_ || audio_end_of_stream_written_);
  SB_DCHECK(!video_dmp_reader_ || video_end_of_stream_written_);

  ASSERT_FALSE(error_occurred_);
  ASSERT_NO_FATAL_FAILURE(WaitForPlayerState(kSbPlayerStateEndOfStream));
}

// static
void SbPlayerTestFixture::DecoderStatusCallback(SbPlayer player,
                                                void* context,
                                                SbMediaType type,
                                                SbPlayerDecoderState state,
                                                int ticket) {
  auto fixture = static_cast<SbPlayerTestFixture*>(context);
  fixture->OnDecoderState(player, type, state, ticket);
}

// static
void SbPlayerTestFixture::PlayerStatusCallback(SbPlayer player,
                                               void* context,
                                               SbPlayerState state,
                                               int ticket) {
  auto fixture = static_cast<SbPlayerTestFixture*>(context);
  fixture->OnPlayerState(player, state, ticket);
}

// static
void SbPlayerTestFixture::ErrorCallback(SbPlayer player,
                                        void* context,
                                        SbPlayerError error,
                                        const char* message) {
  auto fixture = static_cast<SbPlayerTestFixture*>(context);
  fixture->OnError(player, error, message);
}

void SbPlayerTestFixture::OnDecoderState(SbPlayer player,
                                         SbMediaType media_type,
                                         SbPlayerDecoderState state,
                                         int ticket) {
  callback_event_queue_.Put(CallbackEvent(player, media_type, state, ticket));
}

void SbPlayerTestFixture::OnPlayerState(SbPlayer player,
                                        SbPlayerState state,
                                        int ticket) {
  callback_event_queue_.Put(CallbackEvent(player, state, ticket));
}

void SbPlayerTestFixture::OnError(SbPlayer player,
                                  SbPlayerError error,
                                  const char* message) {
  SB_LOG(ERROR) << FormatString("Got SbPlayerError %d with message '%s'", error,
                                message != NULL ? message : "");
  error_occurred_ = true;
}

void SbPlayerTestFixture::Initialize() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  // Initialize drm system.
  if (!key_system_.empty()) {
    drm_system_ = SbDrmCreateSystem(
        key_system_.c_str(), NULL /* context */, DummySessionUpdateRequestFunc,
        DummySessionUpdatedFunc, DummySessionKeyStatusesChangedFunc,
        DummyServerCertificateUpdatedFunc, DummySessionClosedFunc);
    ASSERT_TRUE(SbDrmSystemIsValid(drm_system_));
  }

  // Initialize player.
  auto audio_codec = kSbMediaAudioCodecNone;
  auto video_codec = kSbMediaVideoCodecNone;
  const shared::starboard::media::AudioStreamInfo* audio_stream_info = NULL;

  if (audio_dmp_reader_) {
    audio_codec = audio_dmp_reader_->audio_codec();
    audio_stream_info = &audio_dmp_reader_->audio_stream_info();
  }
  if (video_dmp_reader_) {
    video_codec = video_dmp_reader_->video_codec();
  }
  player_ = CallSbPlayerCreate(
      fake_graphics_context_provider_.window(), video_codec, audio_codec,
      drm_system_, audio_stream_info, "", DummyDeallocateSampleFunc,
      DecoderStatusCallback, PlayerStatusCallback, ErrorCallback, this,
      output_mode_, fake_graphics_context_provider_.decoder_target_provider());
  ASSERT_TRUE(SbPlayerIsValid(player_));
  ASSERT_NO_FATAL_FAILURE(WaitForPlayerState(kSbPlayerStateInitialized));
  ASSERT_NO_FATAL_FAILURE(Seek(0));
  SbPlayerSetPlaybackRate(player_, 1.0);
  SbPlayerSetVolume(player_, 1.0);
}

void SbPlayerTestFixture::TearDown() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  // We should always destroy |player_| and |drm_system_|, no matter if there's
  // any unexpected player error.
  if (SbPlayerIsValid(player_)) {
    destroy_player_called_ = true;
    SbPlayerDestroy(player_);
  }
  if (SbDrmSystemIsValid(drm_system_)) {
    SbDrmDestroySystem(drm_system_);
  }

  // We expect player resources are released and all events are sent already
  // after SbPlayerDestroy() finishes.
  while (callback_event_queue_.Size() > 0) {
    ASSERT_NO_FATAL_FAILURE(WaitAndProcessNextEvent());
  }
  ASSERT_TRUE(HasReceivedPlayerState(kSbPlayerStateDestroyed));
  ASSERT_FALSE(error_occurred_);

  player_ = kSbPlayerInvalid;
  drm_system_ = kSbDrmSystemInvalid;
}

void SbPlayerTestFixture::WriteSamples(SbMediaType media_type,
                                       int start_index,
                                       int samples_to_write) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK(start_index >= 0);
  SB_DCHECK(samples_to_write > 0);
  SB_DCHECK(SbPlayerIsValid(player_));
  SB_DCHECK(samples_to_write <=
            SbPlayerGetMaximumNumberOfSamplesPerWrite(player_, media_type));

  if (media_type == kSbMediaTypeAudio) {
    SB_DCHECK(audio_dmp_reader_);
    SB_DCHECK(start_index + samples_to_write <=
              audio_dmp_reader_->number_of_audio_buffers());
    CallSbPlayerWriteSamples(player_, kSbMediaTypeAudio,
                             audio_dmp_reader_.get(), start_index,
                             samples_to_write);
    can_accept_more_audio_data_ = false;
  } else {
    SB_DCHECK(media_type == kSbMediaTypeVideo);
    SB_DCHECK(video_dmp_reader_);
    SB_DCHECK(start_index + samples_to_write <=
              video_dmp_reader_->number_of_video_buffers());
    CallSbPlayerWriteSamples(player_, kSbMediaTypeVideo,
                             video_dmp_reader_.get(), start_index,
                             samples_to_write);
    can_accept_more_video_data_ = false;
  }
}

void SbPlayerTestFixture::WriteEndOfStream(SbMediaType media_type) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK(SbPlayerIsValid(player_));

  if (media_type == kSbMediaTypeAudio) {
    SB_DCHECK(audio_dmp_reader_);
    SB_DCHECK(!audio_end_of_stream_written_);
    SbPlayerWriteEndOfStream(player_, kSbMediaTypeAudio);
    can_accept_more_audio_data_ = false;
    audio_end_of_stream_written_ = true;
  } else {
    SB_DCHECK(media_type == kSbMediaTypeVideo);
    SB_DCHECK(video_dmp_reader_);
    SB_DCHECK(!video_end_of_stream_written_);
    SbPlayerWriteEndOfStream(player_, kSbMediaTypeVideo);
    can_accept_more_video_data_ = false;
    video_end_of_stream_written_ = true;
  }
}

void SbPlayerTestFixture::WaitAndProcessNextEvent(SbTime timeout) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  auto event = callback_event_queue_.GetTimed(timeout);

  // Ignore callback events for previous Seek().
  if (event.ticket != ticket_) {
    return;
  }

  switch (event.event_type) {
    case CallbackEventType::kEmptyEvent:
      break;
    case CallbackEventType::kDecoderStateEvent: {
      ASSERT_EQ(event.player, player_);
      // Callbacks may be in-flight at the time that the player is destroyed by
      // a call to |SbPlayerDestroy|. In this case, the callbacks are ignored.
      // However no new callbacks are expected after receiving the player status
      // |kSbPlayerStateDestroyed|.
      ASSERT_FALSE(HasReceivedPlayerState(kSbPlayerStateDestroyed));
      // There's only one valid SbPlayerDecoderState. The received decoder state
      // must be kSbPlayerDecoderStateNeedsData.
      ASSERT_EQ(event.decoder_state, kSbPlayerDecoderStateNeedsData);
      if (event.media_type == kSbMediaTypeAudio) {
        ASSERT_FALSE(can_accept_more_audio_data_);
        can_accept_more_audio_data_ = true;
      } else {
        ASSERT_TRUE(event.media_type == kSbMediaTypeVideo);
        ASSERT_FALSE(can_accept_more_video_data_);
        can_accept_more_video_data_ = true;
      }
      break;
    }
    case CallbackEventType::kPlayerStateEvent: {
      ASSERT_EQ(event.player, player_);
      ASSERT_NO_FATAL_FAILURE(AssertPlayerStateIsValid(event.player_state));
      player_state_set_.insert(event.player_state);
      break;
    }
  }
  ASSERT_FALSE(error_occurred_);
}

void SbPlayerTestFixture::WaitForDecoderStateNeedsData(const SbTime timeout) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK(!can_accept_more_audio_data_ || !can_accept_more_video_data_);

  bool old_can_accept_more_audio_data = can_accept_more_audio_data_;
  bool old_can_accept_more_video_data = can_accept_more_video_data_;

  SbTimeMonotonic start = SbTimeGetMonotonicNow();
  do {
    ASSERT_FALSE(error_occurred_);
    GetDecodeTargetWhenSupported();
    ASSERT_NO_FATAL_FAILURE(WaitAndProcessNextEvent());
    if (old_can_accept_more_audio_data != can_accept_more_audio_data_ ||
        old_can_accept_more_video_data != can_accept_more_video_data_) {
      return;
    }
  } while (SbTimeGetMonotonicNow() - start < timeout);

  FAIL() << "WaitForDecoderStateNeedsData() did not receive expected state.";
}

void SbPlayerTestFixture::WaitForPlayerState(const SbPlayerState desired_state,
                                             const SbTime timeout) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  if (HasReceivedPlayerState(desired_state)) {
    return;
  }
  SbTimeMonotonic start = SbTimeGetMonotonicNow();
  do {
    ASSERT_FALSE(error_occurred_);
    ASSERT_NO_FATAL_FAILURE(GetDecodeTargetWhenSupported());
    ASSERT_NO_FATAL_FAILURE(WaitAndProcessNextEvent());
    if (HasReceivedPlayerState(desired_state)) {
      return;
    }
  } while (SbTimeGetMonotonicNow() - start < timeout);

  FAIL() << "WaitForPlayerState() did not receive expected state.";
}

void SbPlayerTestFixture::GetDecodeTargetWhenSupported() {
  if (!SbPlayerIsValid(player_)) {
    return;
  }
#if SB_HAS(GLES2)
  fake_graphics_context_provider_.RunOnGlesContextThread([&]() {
    ASSERT_TRUE(SbPlayerIsValid(player_));
    if (output_mode_ != kSbPlayerOutputModeDecodeToTexture) {
      ASSERT_EQ(SbPlayerGetCurrentFrame(player_), kSbDecodeTargetInvalid);
      return;
    }
    ASSERT_EQ(output_mode_, kSbPlayerOutputModeDecodeToTexture);
    SbDecodeTarget frame = SbPlayerGetCurrentFrame(player_);
    if (SbDecodeTargetIsValid(frame)) {
      SbDecodeTargetRelease(frame);
    }
  });
#endif  // SB_HAS(GLES2)
}

void SbPlayerTestFixture::AssertPlayerStateIsValid(SbPlayerState state) const {
  // Note: it is possible to receive the same state that has been previously
  // received in the case of multiple Seek() calls. Prior to any Seek commands
  // issued in this test, we should reset the |player_state_set_| member.
  ASSERT_FALSE(HasReceivedPlayerState(state));

  switch (state) {
    case kSbPlayerStateInitialized:
      // No other states have been received before getting Initialized.
      ASSERT_TRUE(player_state_set_.empty());
      return;
    case kSbPlayerStatePrerolling:
      ASSERT_TRUE(HasReceivedPlayerState(kSbPlayerStateInitialized));
      ASSERT_FALSE(HasReceivedPlayerState(kSbPlayerStateDestroyed));
      return;
    case kSbPlayerStatePresenting:
      ASSERT_TRUE(HasReceivedPlayerState(kSbPlayerStateInitialized));
      ASSERT_TRUE(HasReceivedPlayerState(kSbPlayerStatePrerolling));
      ASSERT_FALSE(HasReceivedPlayerState(kSbPlayerStateDestroyed));
      return;
    case kSbPlayerStateEndOfStream:
      if (audio_dmp_reader_) {
        ASSERT_TRUE(audio_end_of_stream_written_);
      }
      if (video_dmp_reader_) {
        ASSERT_TRUE(video_end_of_stream_written_);
      }
      ASSERT_TRUE(HasReceivedPlayerState(kSbPlayerStateInitialized));
      ASSERT_TRUE(HasReceivedPlayerState(kSbPlayerStatePrerolling));
      ASSERT_FALSE(HasReceivedPlayerState(kSbPlayerStateDestroyed));
      return;
    case kSbPlayerStateDestroyed:
      // Nothing stops the user of the player from destroying the player during
      // any of the previous states.
      ASSERT_TRUE(destroy_player_called_);
      return;
  }
  FAIL() << "Received an invalid SbPlayerState.";
}

}  // namespace nplb
}  // namespace starboard
