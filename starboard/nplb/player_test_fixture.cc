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

namespace starboard {
namespace nplb {

using shared::starboard::player::video_dmp::VideoDmpReader;
using testing::FakeGraphicsContextProvider;

#define PLAYER_FIXTURE_ASSERT(condition, error_message)                   \
  if (!(condition) && !error_occurred_.exchange(true)) {                  \
    ScopedLock scoped_lock(error_message_mutex_);                         \
    error_message_ =                                                      \
        FormatString("SbPlayerTestFixture failure at %s(%d): ", __FILE__, \
                     __LINE__) +                                          \
        (error_message);                                                  \
    return;                                                               \
  }

#define PLAYER_FIXTURE_FAIL(error_message) \
  PLAYER_FIXTURE_ASSERT(false, error_message)

#define PLAYER_FIXTURE_ABORT_ON_ERROR(statement) \
  { statement; }                                 \
  if (error_occurred_) {                         \
    return;                                      \
  }

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
    : output_mode_(std::get<2>(config)) {
  SB_DCHECK(output_mode_ == kSbPlayerOutputModeDecodeToTexture ||
            output_mode_ == kSbPlayerOutputModePunchOut);

  const char* audio_dmp_filename = std::get<0>(config);
  const char* video_dmp_filename = std::get<1>(config);

  if (audio_dmp_filename && strlen(audio_dmp_filename) > 0) {
    audio_dmp_reader_.reset(new VideoDmpReader(audio_dmp_filename));
  }
  if (video_dmp_filename && strlen(video_dmp_filename) > 0) {
    video_dmp_reader_.reset(new VideoDmpReader(video_dmp_filename));
  }
}

SbPlayerTestFixture::~SbPlayerTestFixture() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK(!SbPlayerIsValid(player_))
      << "TearDownPlayer() needs to be called to destroy SbPlayer.";
}

void SbPlayerTestFixture::InitializePlayer() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK(!error_occurred_);

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
      kSbDrmSystemInvalid, audio_stream_info, "", DummyDeallocateSampleFunc,
      DecoderStatusCallback, PlayerStatusCallback, ErrorCallback, this,
      output_mode_, fake_graphics_context_provider_.decoder_target_provider());
  PLAYER_FIXTURE_ASSERT(SbPlayerIsValid(player_), "Failed to create player.");
  PLAYER_FIXTURE_ABORT_ON_ERROR(WaitForPlayerState(kSbPlayerStateInitialized));
  PLAYER_FIXTURE_ABORT_ON_ERROR(Seek(0));
  SbPlayerSetPlaybackRate(player_, 1.0);
  SbPlayerSetVolume(player_, 1.0);
}

void SbPlayerTestFixture::TearDownPlayer() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  if (!SbPlayerIsValid(player_)) {
    return;
  }

  destroy_player_called_ = true;
  SbPlayerDestroy(player_);

  // We expect player resources are released and all events are sent already
  // after SbPlayerDestroy() finishes.
  while (callback_event_queue_.Size() > 0) {
    PLAYER_FIXTURE_ABORT_ON_ERROR(WaitAndProcessNextEvent());
  }

  PLAYER_FIXTURE_ASSERT(
      HasReceivedPlayerState(kSbPlayerStateDestroyed),
      "|kSbPlayerStateDestroyed| was not received after destroying player.");

  player_ = kSbPlayerInvalid;
}

void SbPlayerTestFixture::Seek(const SbTime time) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK(SbPlayerIsValid(player_));
  if (error_occurred_) {
    return;
  }

  PLAYER_FIXTURE_ASSERT(!HasReceivedPlayerState(kSbPlayerStateDestroyed),
                        "Wrong player state on Seek(). "
                        "|kSbPlayerStateDestroyed| has been received.");
  PLAYER_FIXTURE_ASSERT(HasReceivedPlayerState(kSbPlayerStateInitialized),
                        "Wrong player state on Seek(). "
                        "|kSbPlayerStateInitialized| has not been received.");

  can_accept_more_audio_data_ = false;
  can_accept_more_video_data_ = false;
  player_state_set_.clear();
  player_state_set_.insert(kSbPlayerStateInitialized);
  audio_end_of_stream_written_ = false;
  video_end_of_stream_written_ = false;

#if SB_API_VERSION >= SB_MEDIA_ENHANCED_AUDIO_API_VERSION
  SbPlayerSeek(player_, time, ++ticket_);
#else   // SB_API_VERSION >= SB_MEDIA_ENHANCED_AUDIO_API_VERSION
  SbPlayerSeek2(player_, time, ++ticket_);
#endif  // SB_API_VERSION >= SB_MEDIA_ENHANCED_AUDIO_API_VERSION
}

void SbPlayerTestFixture::Write(const AudioSamples& audio_samples) {
  Write(audio_samples, VideoSamples());
}

void SbPlayerTestFixture::Write(const VideoSamples& video_samples) {
  Write(AudioSamples(), video_samples);
}

void SbPlayerTestFixture::Write(const AudioSamples& audio_samples,
                                const VideoSamples& video_samples) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK(SbPlayerIsValid(player_));
  SB_DCHECK(!audio_end_of_stream_written_);
  SB_DCHECK(!video_end_of_stream_written_);

  if (error_occurred_) {
    return;
  }

  int audio_start_index = audio_samples.start_index();
  int audio_samples_to_write = audio_samples.samples_to_write();
  int video_start_index = video_samples.start_index();
  int video_samples_to_write = video_samples.samples_to_write();
  bool write_audio_eos = audio_samples.write_eos();
  bool write_video_eos = video_samples.write_eos();

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
    PLAYER_FIXTURE_ABORT_ON_ERROR(WaitForDecoderStateNeedsData());
    if (can_accept_more_audio_data_ && has_more_audio) {
      if (audio_samples_to_write > 0) {
        auto samples_to_write =
            std::min(max_audio_samples_per_write, audio_samples_to_write);
        PLAYER_FIXTURE_ABORT_ON_ERROR(WriteSamples(
            kSbMediaTypeAudio, audio_start_index, samples_to_write));
        audio_start_index += samples_to_write;
        audio_samples_to_write -= samples_to_write;
      } else if (!audio_end_of_stream_written_ && write_audio_eos) {
        PLAYER_FIXTURE_ABORT_ON_ERROR(WriteEndOfStream(kSbMediaTypeAudio));
      }
      has_more_audio = audio_samples_to_write > 0 ||
                       (!audio_end_of_stream_written_ && write_audio_eos);
    }

    if (can_accept_more_video_data_ && has_more_video) {
      if (video_samples_to_write > 0) {
        auto samples_to_write =
            std::min(max_video_samples_per_write, video_samples_to_write);
        PLAYER_FIXTURE_ABORT_ON_ERROR(WriteSamples(
            kSbMediaTypeVideo, video_start_index, samples_to_write));
        video_start_index += samples_to_write;
        video_samples_to_write -= samples_to_write;
      } else if (!video_end_of_stream_written_ && write_video_eos) {
        PLAYER_FIXTURE_ABORT_ON_ERROR(WriteEndOfStream(kSbMediaTypeVideo));
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

  if (error_occurred_) {
    return;
  }

  PLAYER_FIXTURE_ABORT_ON_ERROR(WaitForPlayerState(kSbPlayerStateEndOfStream));
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
  PLAYER_FIXTURE_FAIL(FormatString("Got SbPlayerError %d with message '%s'",
                                   error, message != NULL ? message : ""));
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
  if (error_occurred_ || event.event_type == CallbackEventType::kEmptyEvent) {
    return;
  }

  // Ignore callback events for previous Seek().
  if (event.ticket != ticket_) {
    return;
  }

  PLAYER_FIXTURE_ASSERT(event.player == player_,
                        "Wrong |player| was received.");

  switch (event.event_type) {
    case CallbackEventType::kEmptyEvent:
      break;
    case CallbackEventType::kDecoderStateEvent: {
      // Callbacks may be in-flight at the time that the player is destroyed by
      // a call to |SbPlayerDestroy|. In this case, the callbacks are ignored.
      // However no new callbacks are expected after receiving the player status
      // |kSbPlayerStateDestroyed|.
      PLAYER_FIXTURE_ASSERT(!HasReceivedPlayerState(kSbPlayerStateDestroyed),
                            "Decoder callback was invoked after receiving "
                            "|kSbPlayerStateDestroyed|.");
      // There's only one valid SbPlayerDecoderState. The received decoder state
      // must be kSbPlayerDecoderStateNeedsData.
      PLAYER_FIXTURE_ASSERT(
          event.decoder_state == kSbPlayerDecoderStateNeedsData,
          "Wrong SbPlayerDecoderState was received. SbPlayerDecoderState must "
          "be |kSbPlayerDecoderStateNeedsData|.");
      if (event.media_type == kSbMediaTypeAudio) {
        PLAYER_FIXTURE_ASSERT(
            !can_accept_more_audio_data_,
            "Receiving |kSbPlayerDecoderStateNeedsData| more "
            "than once before calling SbPlayerWriteSamples().");
        can_accept_more_audio_data_ = true;
      } else {
        PLAYER_FIXTURE_ASSERT(
            event.media_type == kSbMediaTypeVideo,
            "Wrong SbMediaType was received. SbMediaType must be either "
            "|kSbMediaTypeAudio| or |kSbMediaTypeVideo|.");
        PLAYER_FIXTURE_ASSERT(
            !can_accept_more_video_data_,
            "Receiving |kSbPlayerDecoderStateNeedsData| more "
            "than once before calling SbPlayerWriteSamples().");
        can_accept_more_video_data_ = true;
      }
      break;
    }
    case CallbackEventType::kPlayerStateEvent: {
      PLAYER_FIXTURE_ABORT_ON_ERROR(
          AssertPlayerStateIsValid(event.player_state));
      player_state_set_.insert(event.player_state);
      break;
    }
  }
}

void SbPlayerTestFixture::WaitForDecoderStateNeedsData(const SbTime timeout) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK(!can_accept_more_audio_data_ || !can_accept_more_video_data_);

  bool old_can_accept_more_audio_data = can_accept_more_audio_data_;
  bool old_can_accept_more_video_data = can_accept_more_video_data_;

  SbTimeMonotonic start = SbTimeGetMonotonicNow();
  do {
    PLAYER_FIXTURE_ABORT_ON_ERROR(GetDecodeTargetWhenSupported());
    PLAYER_FIXTURE_ABORT_ON_ERROR(WaitAndProcessNextEvent());
    if (old_can_accept_more_audio_data != can_accept_more_audio_data_ ||
        old_can_accept_more_video_data != can_accept_more_video_data_) {
      return;
    }
  } while (SbTimeGetMonotonicNow() - start < timeout);

  PLAYER_FIXTURE_FAIL(
      "WaitForDecoderStateNeedsData() did not receive expected state.");
}

void SbPlayerTestFixture::WaitForPlayerState(const SbPlayerState desired_state,
                                             const SbTime timeout) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  if (HasReceivedPlayerState(desired_state)) {
    return;
  }
  SbTimeMonotonic start = SbTimeGetMonotonicNow();
  do {
    PLAYER_FIXTURE_ABORT_ON_ERROR(GetDecodeTargetWhenSupported());
    PLAYER_FIXTURE_ABORT_ON_ERROR(WaitAndProcessNextEvent());
    if (HasReceivedPlayerState(desired_state)) {
      return;
    }
  } while (SbTimeGetMonotonicNow() - start < timeout);

  PLAYER_FIXTURE_FAIL("WaitForPlayerState() did not receive expected state.");
}

void SbPlayerTestFixture::GetDecodeTargetWhenSupported() {
  if (!SbPlayerIsValid(player_)) {
    return;
  }
#if SB_HAS(GLES2)
  fake_graphics_context_provider_.RunOnGlesContextThread([&]() {
    if (output_mode_ != kSbPlayerOutputModeDecodeToTexture) {
      PLAYER_FIXTURE_ASSERT(
          SbPlayerGetCurrentFrame(player_) == kSbDecodeTargetInvalid,
          "SbPlayerGetCurrentFrame() must return |kSbDecodeTargetInvalid| when "
          "output mode is not |kSbPlayerOutputModeDecodeToTexture|.");
      return;
    }
    SbDecodeTarget frame = SbPlayerGetCurrentFrame(player_);
    if (SbDecodeTargetIsValid(frame)) {
      SbDecodeTargetRelease(frame);
    }
  });
#endif  // SB_HAS(GLES2)
}

void SbPlayerTestFixture::AssertPlayerStateIsValid(SbPlayerState state) {
  // Note: it is possible to receive the same state that has been previously
  // received in the case of multiple Seek() calls. Prior to any Seek commands
  // issued in this test, we should reset the |player_state_set_| member.
  PLAYER_FIXTURE_ASSERT(!HasReceivedPlayerState(state),
                        "Receiving duplicate player state.");

  switch (state) {
    case kSbPlayerStateInitialized:
      PLAYER_FIXTURE_ASSERT(player_state_set_.empty(),
                            "|kSbPlayerStateInitialized| should be received "
                            "before any other state.");
      return;
    case kSbPlayerStatePrerolling:
      PLAYER_FIXTURE_ASSERT(
          HasReceivedPlayerState(kSbPlayerStateInitialized),
          "Receiving |kSbPlayerStatePrerolling| before player is initialized.");
      PLAYER_FIXTURE_ASSERT(
          !HasReceivedPlayerState(kSbPlayerStateDestroyed),
          "Receiving |kSbPlayerStatePrerolling| after player is destroyed.");
      return;
    case kSbPlayerStatePresenting:
      PLAYER_FIXTURE_ASSERT(
          HasReceivedPlayerState(kSbPlayerStateInitialized),
          "Receiving |kSbPlayerStatePresenting| before player is initialized.");
      PLAYER_FIXTURE_ASSERT(
          HasReceivedPlayerState(kSbPlayerStatePrerolling),
          "Receiving |kSbPlayerStatePresenting| before player is prerolled.");
      PLAYER_FIXTURE_ASSERT(
          !HasReceivedPlayerState(kSbPlayerStateDestroyed),
          "Receiving |kSbPlayerStatePresenting| after player is destroyed.");
      return;
    case kSbPlayerStateEndOfStream:
      PLAYER_FIXTURE_ASSERT(HasReceivedPlayerState(kSbPlayerStateInitialized),
                            "Receiving |kSbPlayerStateEndOfStream| before "
                            "player is initialized.");
      PLAYER_FIXTURE_ASSERT(
          HasReceivedPlayerState(kSbPlayerStatePrerolling),
          "Receiving |kSbPlayerStateEndOfStream| before player is prerolled.");
      PLAYER_FIXTURE_ASSERT(
          !HasReceivedPlayerState(kSbPlayerStateDestroyed),
          "Receiving |kSbPlayerStateEndOfStream| after player is destroyed.");
      if (audio_dmp_reader_) {
        PLAYER_FIXTURE_ASSERT(audio_end_of_stream_written_,
                              "Receiving |kSbPlayerStateEndOfStream| before "
                              "end of stream is written.");
      }
      if (video_dmp_reader_) {
        PLAYER_FIXTURE_ASSERT(video_end_of_stream_written_,
                              "Receiving |kSbPlayerStateEndOfStream| before "
                              "end of stream is written.");
      }
      return;
    case kSbPlayerStateDestroyed:
      // Nothing stops the user of the player from destroying the player during
      // any of the previous states.
      PLAYER_FIXTURE_ASSERT(destroy_player_called_,
                            "Receiving |kSbPlayerStateDestroyed| before "
                            "SbPlayerDestroy() is called.");
      return;
  }
  PLAYER_FIXTURE_FAIL("Received an invalid SbPlayerState.");
}

}  // namespace nplb
}  // namespace starboard
