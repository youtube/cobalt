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
#include <vector>

#include "starboard/common/check_op.h"
#include "starboard/common/string.h"
#include "starboard/common/time.h"
#include "starboard/nplb/drm_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {

using ::starboard::FakeGraphicsContextProvider;
using ::starboard::VideoDmpReader;

using GroupedSamples = SbPlayerTestFixture::GroupedSamples;
using AudioSamplesDescriptor = GroupedSamples::AudioSamplesDescriptor;
using VideoSamplesDescriptor = GroupedSamples::VideoSamplesDescriptor;

// TODO: Refine the implementation.
class SbPlayerTestFixture::GroupedSamplesIterator {
 public:
  explicit GroupedSamplesIterator(const GroupedSamples& grouped_samples)
      : grouped_samples_(grouped_samples) {}

  bool HasMoreAudio() const {
    return static_cast<size_t>(audio_samples_index_) <
           grouped_samples_.audio_samples_.size();
  }

  bool HasMoreVideo() const {
    return static_cast<size_t>(video_samples_index_) <
           grouped_samples_.video_samples_.size();
  }

  AudioSamplesDescriptor GetCurrentAudioSamplesToWrite() const {
    SB_DCHECK(HasMoreAudio());
    AudioSamplesDescriptor descriptor =
        grouped_samples_.audio_samples_[audio_samples_index_];
    descriptor.start_index += current_written_audio_samples_;
    descriptor.samples_count -= current_written_audio_samples_;
    return descriptor;
  }

  VideoSamplesDescriptor GetCurrentVideoSamplesToWrite() const {
    SB_DCHECK(HasMoreVideo());
    VideoSamplesDescriptor descriptor =
        grouped_samples_.video_samples_[video_samples_index_];
    descriptor.start_index += current_written_video_samples_;
    descriptor.samples_count -= current_written_video_samples_;
    return descriptor;
  }

  void AdvanceAudio(int samples_count) {
    SB_DCHECK(HasMoreAudio());
    if (grouped_samples_.audio_samples_[audio_samples_index_]
            .is_end_of_stream) {
      // For EOS, |samples_count| must be 1.
      SB_DCHECK_EQ(samples_count, 1);
      SB_DCHECK_EQ(current_written_audio_samples_, 0);
      audio_samples_index_++;
      return;
    }

    SB_DCHECK(
        current_written_audio_samples_ + samples_count <=
        grouped_samples_.audio_samples_[audio_samples_index_].samples_count);

    current_written_audio_samples_ += samples_count;
    if (current_written_audio_samples_ ==
        grouped_samples_.audio_samples_[audio_samples_index_].samples_count) {
      audio_samples_index_++;
      current_written_audio_samples_ = 0;
    }
  }

  void AdvanceVideo(int samples_count) {
    SB_DCHECK(HasMoreVideo());
    if (grouped_samples_.video_samples_[video_samples_index_]
            .is_end_of_stream) {
      // For EOS, |samples_count| must be 1.
      SB_DCHECK_EQ(samples_count, 1);
      SB_DCHECK_EQ(current_written_video_samples_, 0);
      video_samples_index_++;
      return;
    }

    SB_DCHECK(
        current_written_video_samples_ + samples_count <=
        grouped_samples_.video_samples_[video_samples_index_].samples_count);

    current_written_video_samples_ += samples_count;
    if (current_written_video_samples_ ==
        grouped_samples_.video_samples_[video_samples_index_].samples_count) {
      video_samples_index_++;
      current_written_video_samples_ = 0;
    }
  }

 private:
  const GroupedSamples& grouped_samples_;
  int audio_samples_index_ = 0;
  int current_written_audio_samples_ = 0;
  int video_samples_index_ = 0;
  int current_written_video_samples_ = 0;
};

GroupedSamples& GroupedSamples::AddAudioSamples(int start_index,
                                                int number_of_samples) {
  AddAudioSamples(start_index, number_of_samples, 0, 0, 0);
  return *this;
}

GroupedSamples& GroupedSamples::AddAudioSamples(
    int start_index,
    int number_of_samples,
    int64_t timestamp_offset,
    int64_t discarded_duration_from_front,
    int64_t discarded_duration_from_back) {
  SB_DCHECK_GE(start_index, 0);
  SB_DCHECK_GE(number_of_samples, 0);
  SB_DCHECK(audio_samples_.empty() || !audio_samples_.back().is_end_of_stream);
  // Currently, the implementation only supports writing one sample at a time
  // if |discarded_duration_from_front| or |discarded_duration_from_back| is not
  // 0.
  SB_DCHECK(discarded_duration_from_front == 0 || number_of_samples == 1);
  SB_DCHECK(discarded_duration_from_back == 0 || number_of_samples == 1);

  AudioSamplesDescriptor descriptor;
  descriptor.start_index = start_index;
  descriptor.samples_count = number_of_samples;
  descriptor.timestamp_offset = timestamp_offset;
  descriptor.discarded_duration_from_front = discarded_duration_from_front;
  descriptor.discarded_duration_from_back = discarded_duration_from_back;
  audio_samples_.push_back(descriptor);

  return *this;
}

GroupedSamples& GroupedSamples::AddAudioEOS() {
  SB_DCHECK(audio_samples_.empty() || !audio_samples_.back().is_end_of_stream);

  AudioSamplesDescriptor descriptor;
  descriptor.is_end_of_stream = true;
  audio_samples_.push_back(descriptor);

  return *this;
}

GroupedSamples& GroupedSamples::AddVideoSamples(int start_index,
                                                int number_of_samples) {
  SB_DCHECK_GE(start_index, 0);
  SB_DCHECK_GE(number_of_samples, 0);
  SB_DCHECK(video_samples_.empty() || !video_samples_.back().is_end_of_stream);

  VideoSamplesDescriptor descriptor;
  descriptor.start_index = start_index;
  descriptor.samples_count = number_of_samples;
  video_samples_.push_back(descriptor);

  return *this;
}

GroupedSamples& GroupedSamples::AddVideoEOS() {
  SB_DCHECK(video_samples_.empty() || !video_samples_.back().is_end_of_stream);

  VideoSamplesDescriptor descriptor;
  descriptor.is_end_of_stream = true;
  video_samples_.push_back(descriptor);

  return *this;
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

SbPlayerTestFixture::SbPlayerTestFixture(
    const SbPlayerTestConfig& config,
    FakeGraphicsContextProvider* fake_graphics_context_provider)
    : output_mode_(config.output_mode),
      key_system_(config.key_system),
      max_video_capabilities_(config.max_video_capabilities),
      fake_graphics_context_provider_(fake_graphics_context_provider) {
  SB_DCHECK(output_mode_ == kSbPlayerOutputModeDecodeToTexture ||
            output_mode_ == kSbPlayerOutputModePunchOut);

  const char* audio_dmp_filename = config.audio_filename;
  const char* video_dmp_filename = config.video_filename;

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
  SB_CHECK(thread_checker_.CalledOnValidThread());

  TearDown();
}

void SbPlayerTestFixture::Seek(const int64_t time) {
  SB_CHECK(thread_checker_.CalledOnValidThread());
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

  SbPlayerSeek(player_, time, ++ticket_);
}

void SbPlayerTestFixture::Write(const GroupedSamples& grouped_samples) {
  SB_CHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK(SbPlayerIsValid(player_));
  SB_DCHECK(!audio_end_of_stream_written_);
  SB_DCHECK(!video_end_of_stream_written_);

  ASSERT_FALSE(error_occurred_);

  int max_audio_samples_per_write =
      SbPlayerGetMaximumNumberOfSamplesPerWrite(player_, kSbMediaTypeAudio);
  int max_video_samples_per_write =
      SbPlayerGetMaximumNumberOfSamplesPerWrite(player_, kSbMediaTypeVideo);

  GroupedSamplesIterator iterator(grouped_samples);
  SB_DCHECK(!iterator.HasMoreAudio() || audio_dmp_reader_);
  SB_DCHECK(!iterator.HasMoreVideo() || video_dmp_reader_);

  const int64_t kDefaultWriteTimeout = 5'000'000LL;  // 5 seconds

  int64_t start = starboard::CurrentMonotonicTime();
  while (starboard::CurrentMonotonicTime() - start < kDefaultWriteTimeout) {
    if (CanWriteMoreAudioData() && iterator.HasMoreAudio()) {
      auto descriptor = iterator.GetCurrentAudioSamplesToWrite();
      if (descriptor.is_end_of_stream) {
        SB_DCHECK(!audio_end_of_stream_written_);
        ASSERT_NO_FATAL_FAILURE(WriteEndOfStream(kSbMediaTypeAudio));
        iterator.AdvanceAudio(1);
      } else {
        SB_DCHECK_GT(descriptor.samples_count, 0);
        SB_DCHECK(static_cast<size_t>(descriptor.start_index +
                                      descriptor.samples_count) <
                  audio_dmp_reader_->number_of_audio_buffers())
            << "Audio dmp file is not long enough to finish the test.";

        auto samples_to_write =
            std::min(max_audio_samples_per_write, descriptor.samples_count);
        ASSERT_NO_FATAL_FAILURE(
            WriteAudioSamples(descriptor.start_index, samples_to_write,
                              descriptor.timestamp_offset,
                              descriptor.discarded_duration_from_front,
                              descriptor.discarded_duration_from_back));
        iterator.AdvanceAudio(samples_to_write);
      }
    }
    if (CanWriteMoreVideoData() && iterator.HasMoreVideo()) {
      auto descriptor = iterator.GetCurrentVideoSamplesToWrite();
      if (descriptor.is_end_of_stream) {
        SB_DCHECK(!video_end_of_stream_written_);
        ASSERT_NO_FATAL_FAILURE(WriteEndOfStream(kSbMediaTypeVideo));
        iterator.AdvanceVideo(1);
      } else {
        SB_DCHECK_GT(descriptor.samples_count, 0);
        SB_DCHECK(static_cast<size_t>(descriptor.start_index +
                                      descriptor.samples_count) <
                  video_dmp_reader_->number_of_video_buffers())
            << "Video dmp file is not long enough to finish the test.";

        auto samples_to_write =
            std::min(max_video_samples_per_write, descriptor.samples_count);
        ASSERT_NO_FATAL_FAILURE(
            WriteVideoSamples(descriptor.start_index, samples_to_write));
        iterator.AdvanceVideo(samples_to_write);
      }
    }

    if (iterator.HasMoreAudio() || iterator.HasMoreVideo()) {
      ASSERT_NO_FATAL_FAILURE(WaitForDecoderStateNeedsData());
    } else {
      return;
    }
  }

  FAIL() << "Failed to write all samples.";
}

void SbPlayerTestFixture::WaitForPlayerPresenting() {
  SB_CHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK(SbPlayerIsValid(player_));

  ASSERT_FALSE(error_occurred_);
  ASSERT_NO_FATAL_FAILURE(WaitForPlayerState(kSbPlayerStatePresenting));
}

void SbPlayerTestFixture::WaitForPlayerEndOfStream() {
  SB_CHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK(SbPlayerIsValid(player_));
  SB_DCHECK(!audio_dmp_reader_ || audio_end_of_stream_written_);
  SB_DCHECK(!video_dmp_reader_ || video_end_of_stream_written_);

  ASSERT_FALSE(error_occurred_);
  ASSERT_NO_FATAL_FAILURE(WaitForPlayerState(kSbPlayerStateEndOfStream));
}

int64_t SbPlayerTestFixture::GetCurrentMediaTime() const {
  SbPlayerInfo info = {};
  SbPlayerGetInfo(player_, &info);
  return info.current_media_timestamp;
}

void SbPlayerTestFixture::SetAudioWriteDuration(int64_t duration) {
  SB_CHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK_GT(duration, 0);
  audio_write_duration_ = duration;
}

int64_t SbPlayerTestFixture::GetAudioSampleTimestamp(int index) const {
  SB_DCHECK(HasAudio());
  SB_DCHECK(static_cast<size_t>(index) <
            audio_dmp_reader_->number_of_audio_buffers());
  return audio_dmp_reader_->GetPlayerSampleInfo(kSbMediaTypeAudio, index)
      .timestamp;
}

int SbPlayerTestFixture::ConvertDurationToAudioBufferCount(
    int64_t duration) const {
  SB_DCHECK(HasAudio());
  SB_DCHECK(audio_dmp_reader_->number_of_audio_buffers());
  return duration * audio_dmp_reader_->number_of_audio_buffers() /
         audio_dmp_reader_->audio_duration();
}

int SbPlayerTestFixture::ConvertDurationToVideoBufferCount(
    int64_t duration) const {
  SB_DCHECK(HasVideo());
  SB_DCHECK(video_dmp_reader_->number_of_video_buffers());
  return duration * video_dmp_reader_->number_of_video_buffers() /
         video_dmp_reader_->video_duration();
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
  SB_LOG(ERROR) << starboard::FormatString(
      "Got SbPlayerError %d with message '%s'", error,
      message != NULL ? message : "");
  error_occurred_ = true;
}

void SbPlayerTestFixture::Initialize() {
  SB_CHECK(thread_checker_.CalledOnValidThread());

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
  const starboard::AudioStreamInfo* audio_stream_info = nullptr;

  if (audio_dmp_reader_) {
    audio_codec = audio_dmp_reader_->audio_codec();
    audio_stream_info = &audio_dmp_reader_->audio_stream_info();
  }
  if (video_dmp_reader_) {
    video_codec = video_dmp_reader_->video_codec();
  }

  // TODO: refine CallSbPlayerCreate() to use real video sample info.
  player_ = CallSbPlayerCreate(
      fake_graphics_context_provider_->window(), video_codec, audio_codec,
      drm_system_, audio_stream_info, max_video_capabilities_.c_str(),
      DummyDeallocateSampleFunc, DecoderStatusCallback, PlayerStatusCallback,
      ErrorCallback, this, output_mode_,
      fake_graphics_context_provider_->decoder_target_provider());
  ASSERT_TRUE(SbPlayerIsValid(player_));
  ASSERT_NO_FATAL_FAILURE(WaitForPlayerState(kSbPlayerStateInitialized));
  ASSERT_NO_FATAL_FAILURE(Seek(0));
  SbPlayerSetPlaybackRate(player_, 1.0);
  SbPlayerSetVolume(player_, 1.0);
}

void SbPlayerTestFixture::TearDown() {
  SB_CHECK(thread_checker_.CalledOnValidThread());

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

bool SbPlayerTestFixture::CanWriteMoreAudioData() {
  if (!can_accept_more_audio_data_) {
    return false;
  }

  if (!audio_write_duration_) {
    return true;
  }

  return last_written_audio_timestamp_ - GetCurrentMediaTime() <
         audio_write_duration_;
}

bool SbPlayerTestFixture::CanWriteMoreVideoData() {
  return can_accept_more_video_data_;
}

void SbPlayerTestFixture::WriteAudioSamples(
    int start_index,
    int samples_to_write,
    int64_t timestamp_offset,
    int64_t discarded_duration_from_front,
    int64_t discarded_duration_from_back) {
  SB_CHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK(SbPlayerIsValid(player_));
  SB_DCHECK(audio_dmp_reader_);
  SB_DCHECK_GE(start_index, 0);
  SB_DCHECK_GT(samples_to_write, 0);
  SB_DCHECK_LE(samples_to_write, SbPlayerGetMaximumNumberOfSamplesPerWrite(
                                     player_, kSbMediaTypeAudio));
  SB_DCHECK_LT(static_cast<size_t>(start_index + samples_to_write + 1),
               audio_dmp_reader_->number_of_audio_buffers());
  SB_DCHECK(discarded_duration_from_front == 0 || samples_to_write == 1);
  SB_DCHECK(discarded_duration_from_back == 0 || samples_to_write == 1);

  CallSbPlayerWriteSamples(
      player_, kSbMediaTypeAudio, audio_dmp_reader_.get(), start_index,
      samples_to_write, timestamp_offset,
      std::vector<int64_t>(samples_to_write, discarded_duration_from_front),
      std::vector<int64_t>(samples_to_write, discarded_duration_from_back));

  last_written_audio_timestamp_ =
      audio_dmp_reader_
          ->GetPlayerSampleInfo(kSbMediaTypeAudio,
                                start_index + samples_to_write)
          .timestamp;

  can_accept_more_audio_data_ = false;
}

void SbPlayerTestFixture::WriteVideoSamples(int start_index,
                                            int samples_to_write) {
  SB_CHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK_GE(start_index, 0);
  SB_DCHECK_GT(samples_to_write, 0);
  SB_DCHECK(SbPlayerIsValid(player_));
  SB_DCHECK_LE(samples_to_write, SbPlayerGetMaximumNumberOfSamplesPerWrite(
                                     player_, kSbMediaTypeVideo));
  SB_DCHECK(video_dmp_reader_);
  SB_DCHECK_LT(static_cast<size_t>(start_index + samples_to_write),
               video_dmp_reader_->number_of_video_buffers());

  CallSbPlayerWriteSamples(player_, kSbMediaTypeVideo, video_dmp_reader_.get(),
                           start_index, samples_to_write);
  can_accept_more_video_data_ = false;
}

void SbPlayerTestFixture::WriteEndOfStream(SbMediaType media_type) {
  SB_CHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK(SbPlayerIsValid(player_));

  if (media_type == kSbMediaTypeAudio) {
    SB_DCHECK(audio_dmp_reader_);
    SB_DCHECK(!audio_end_of_stream_written_);
    SbPlayerWriteEndOfStream(player_, kSbMediaTypeAudio);
    can_accept_more_audio_data_ = false;
    audio_end_of_stream_written_ = true;
  } else {
    SB_DCHECK_EQ(media_type, kSbMediaTypeVideo);
    SB_DCHECK(video_dmp_reader_);
    SB_DCHECK(!video_end_of_stream_written_);
    SbPlayerWriteEndOfStream(player_, kSbMediaTypeVideo);
    can_accept_more_video_data_ = false;
    video_end_of_stream_written_ = true;
  }
}

void SbPlayerTestFixture::WaitAndProcessNextEvent(int64_t timeout) {
  SB_CHECK(thread_checker_.CalledOnValidThread());

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

void SbPlayerTestFixture::WaitForDecoderStateNeedsData(const int64_t timeout) {
  SB_CHECK(thread_checker_.CalledOnValidThread());

  bool old_can_accept_more_audio_data = can_accept_more_audio_data_;
  bool old_can_accept_more_video_data = can_accept_more_video_data_;

  int64_t start = starboard::CurrentMonotonicTime();
  do {
    ASSERT_FALSE(error_occurred_);
    GetDecodeTargetWhenSupported();
    ASSERT_NO_FATAL_FAILURE(WaitAndProcessNextEvent());
    if (old_can_accept_more_audio_data != can_accept_more_audio_data_ ||
        old_can_accept_more_video_data != can_accept_more_video_data_) {
      return;
    }
  } while (starboard::CurrentMonotonicTime() - start < timeout);
}

void SbPlayerTestFixture::WaitForPlayerState(const SbPlayerState desired_state,
                                             const int64_t timeout) {
  SB_CHECK(thread_checker_.CalledOnValidThread());

  if (HasReceivedPlayerState(desired_state)) {
    return;
  }
  int64_t start = starboard::CurrentMonotonicTime();
  do {
    ASSERT_FALSE(error_occurred_);
    ASSERT_NO_FATAL_FAILURE(GetDecodeTargetWhenSupported());
    ASSERT_NO_FATAL_FAILURE(WaitAndProcessNextEvent());
    if (HasReceivedPlayerState(desired_state)) {
      return;
    }
  } while (starboard::CurrentMonotonicTime() - start < timeout);

  FAIL() << "WaitForPlayerState() did not receive expected state.";
}

void SbPlayerTestFixture::GetDecodeTargetWhenSupported() {
  if (!SbPlayerIsValid(player_)) {
    return;
  }
  fake_graphics_context_provider_->RunOnGlesContextThread([&]() {
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
