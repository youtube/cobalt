// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/player/filter/audio_renderer_internal_impl.h"

#include <algorithm>
#include <string>

#include "starboard/memory.h"
#include "starboard/shared/starboard/media/media_util.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

namespace {

// This class works only when the input format and output format are the same.
// It allows for a simplified AudioRendererImpl implementation by always using a
// resampler.
class IdentityAudioResampler : public AudioResampler {
 public:
  IdentityAudioResampler() : eos_reached_(false) {}
  scoped_refptr<DecodedAudio> Resample(
      const scoped_refptr<DecodedAudio>& audio_data) override {
    SB_DCHECK(!eos_reached_);

    return audio_data;
  }
  scoped_refptr<DecodedAudio> WriteEndOfStream() override {
    SB_DCHECK(!eos_reached_);
    eos_reached_ = true;
    return new DecodedAudio();
  }

 private:
  bool eos_reached_;
};

// AudioRendererImpl uses AudioTimeStretcher internally to adjust to playback
// rate. So we try to use kSbMediaAudioSampleTypeFloat32 and only use
// kSbMediaAudioSampleTypeInt16Deprecated when float32 is not supported.  To use
// kSbMediaAudioSampleTypeFloat32 will cause an extra conversion from float32 to
// int16 before the samples are sent to the audio sink.
SbMediaAudioSampleType GetSinkAudioSampleType(
    AudioRendererSink* audio_renderer_sink) {
  return audio_renderer_sink->IsAudioSampleTypeSupported(
             kSbMediaAudioSampleTypeFloat32)
             ? kSbMediaAudioSampleTypeFloat32
             : kSbMediaAudioSampleTypeInt16Deprecated;
}

}  // namespace

AudioRendererImpl::AudioRendererImpl(
    scoped_ptr<AudioDecoder> decoder,
    scoped_ptr<AudioRendererSink> audio_renderer_sink,
    const SbMediaAudioSampleInfo& audio_sample_info,
    int max_cached_frames,
    int min_frames_per_append)
    : max_cached_frames_(max_cached_frames),
      min_frames_per_append_(min_frames_per_append),
      channels_(audio_sample_info.number_of_channels),
      sink_sample_type_(GetSinkAudioSampleType(audio_renderer_sink.get())),
      bytes_per_frame_(media::GetBytesPerSample(sink_sample_type_) * channels_),
      frame_buffer_(max_cached_frames_ * bytes_per_frame_),
      frames_consumed_set_at_(SbTimeGetMonotonicNow()),
      decoder_(decoder.Pass()),
      process_audio_data_job_(
          std::bind(&AudioRendererImpl::ProcessAudioData, this)),
      audio_renderer_sink_(audio_renderer_sink.Pass()) {
  SB_DLOG(INFO) << "Creating AudioRendererImpl with " << channels_
                << " channels, " << bytes_per_frame_ << " bytes per frame, "
                << max_cached_frames_ << " max cached frames, and "
                << min_frames_per_append_ << " min frames per append.";
  SB_DCHECK(decoder_ != NULL);
  SB_DCHECK(min_frames_per_append_ > 0);
  SB_DCHECK(max_cached_frames_ >= min_frames_per_append_ * 2);

  frame_buffers_[0] = &frame_buffer_[0];

#if SB_PLAYER_FILTER_ENABLE_STATE_CHECK
  Schedule(std::bind(&AudioRendererImpl::CheckAudioSinkStatus, this),
           kCheckAudioSinkStatusInterval);
#endif  // SB_PLAYER_FILTER_ENABLE_STATE_CHECK
}

AudioRendererImpl::~AudioRendererImpl() {
  SB_DLOG(INFO) << "Destroying AudioRendererImpl with " << channels_
                << " channels, " << bytes_per_frame_ << " bytes per frame, "
                << max_cached_frames_ << " max cached frames, and "
                << min_frames_per_append_ << " min frames per append.";
  SB_DCHECK(BelongsToCurrentThread());
}

void AudioRendererImpl::Initialize(const ErrorCB& error_cb,
                                   const PrerolledCB& prerolled_cb,
                                   const EndedCB& ended_cb) {
  SB_DCHECK(error_cb);
  SB_DCHECK(prerolled_cb);
  SB_DCHECK(ended_cb);
  SB_DCHECK(!error_cb_);
  SB_DCHECK(!prerolled_cb_);
  SB_DCHECK(!ended_cb_);

  error_cb_ = error_cb;
  prerolled_cb_ = prerolled_cb;
  ended_cb_ = ended_cb;

  decoder_->Initialize(std::bind(&AudioRendererImpl::OnDecoderOutput, this),
                       error_cb);
}

void AudioRendererImpl::WriteSample(
    const scoped_refptr<InputBuffer>& input_buffer) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(input_buffer);
  SB_DCHECK(can_accept_more_data_);

  if (eos_state_ >= kEOSWrittenToDecoder) {
    SB_LOG(ERROR) << "Appending audio sample at " << input_buffer->timestamp()
                  << " after EOS reached.";
    return;
  }

  can_accept_more_data_ = false;

  decoder_->Decode(input_buffer,
                   std::bind(&AudioRendererImpl::OnDecoderConsumed, this));
  first_input_written_ = true;
}

void AudioRendererImpl::WriteEndOfStream() {
  SB_DCHECK(BelongsToCurrentThread());
  // TODO: Check |can_accept_more_data_| and make WriteEndOfStream() depend on
  // CanAcceptMoreData() or callback.
  // SB_DCHECK(can_accept_more_data_);
  // can_accept_more_data_ = false;

  if (eos_state_ >= kEOSWrittenToDecoder) {
    SB_LOG(ERROR) << "Try to write EOS after EOS is reached";
    return;
  }

  decoder_->WriteEndOfStream();

  ScopedLock lock(mutex_);
  eos_state_ = kEOSWrittenToDecoder;
  first_input_written_ = true;
}

void AudioRendererImpl::SetVolume(double volume) {
  SB_DCHECK(BelongsToCurrentThread());
  audio_renderer_sink_->SetVolume(volume);
}

bool AudioRendererImpl::IsEndOfStreamWritten() const {
  SB_DCHECK(BelongsToCurrentThread());
  return eos_state_ >= kEOSWrittenToDecoder;
}

bool AudioRendererImpl::IsEndOfStreamPlayed() const {
  ScopedLock lock(mutex_);
  return IsEndOfStreamPlayed_Locked();
}

bool AudioRendererImpl::CanAcceptMoreData() const {
  SB_DCHECK(BelongsToCurrentThread());
  return eos_state_ == kEOSNotReceived && can_accept_more_data_ &&
         (!decoder_sample_rate_ || !time_stretcher_.IsQueueFull());
}

void AudioRendererImpl::Play() {
  SB_DCHECK(BelongsToCurrentThread());

  ScopedLock lock(mutex_);
  paused_ = false;
  consume_frames_called_ = false;
}

void AudioRendererImpl::Pause() {
  SB_DCHECK(BelongsToCurrentThread());

  ScopedLock lock(mutex_);
  paused_ = true;
}

void AudioRendererImpl::SetPlaybackRate(double playback_rate) {
  SB_DCHECK(BelongsToCurrentThread());

  ScopedLock lock(mutex_);

  if (playback_rate_ == 0.f && playback_rate > 0.f) {
    consume_frames_called_ = false;
  }

  playback_rate_ = playback_rate;

  audio_renderer_sink_->SetPlaybackRate(playback_rate_ > 0.0 ? 1.0 : 0.0);
  if (audio_renderer_sink_->HasStarted()) {
    // TODO: Remove SetPlaybackRate() support from audio sink as it only need to
    // support play/pause.
    if (playback_rate_ > 0.0) {
      if (process_audio_data_job_token_.is_valid()) {
        RemoveJobByToken(process_audio_data_job_token_);
        process_audio_data_job_token_.ResetToInvalid();
      }
      process_audio_data_job_token_ = Schedule(process_audio_data_job_);
    }
  }
}

void AudioRendererImpl::Seek(SbTime seek_to_time) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(seek_to_time >= 0);

  audio_renderer_sink_->Stop();

  {
    // Set the following states under a lock first to ensure that from now on
    // GetCurrentMediaTime() returns |seeking_to_time_|.
    ScopedLock scoped_lock(mutex_);
    eos_state_ = kEOSNotReceived;
    seeking_to_time_ = std::max<SbTime>(seek_to_time, 0);
    last_media_time_ = seek_to_time;
    ended_cb_called_ = false;
    seeking_ = true;
  }

  // Now the sink is stopped and the callbacks will no longer be called, so the
  // following modifications are safe without lock.
  if (resampler_) {
    resampler_.reset();
    time_stretcher_.FlushBuffers();
  }

  total_frames_sent_to_sink_ = 0;
  total_frames_consumed_by_sink_ = 0;
  frames_consumed_by_sink_since_last_get_current_time_ = 0;
  pending_decoder_outputs_ = 0;
  audio_frame_tracker_.Reset();
  frames_consumed_set_at_ = SbTimeGetMonotonicNow();
  can_accept_more_data_ = true;
  process_audio_data_job_token_.ResetToInvalid();

  is_eos_reached_on_sink_thread_ = false;
  is_playing_on_sink_thread_ = false;
  frames_in_buffer_on_sink_thread_ = 0;
  offset_in_frames_on_sink_thread_ = 0;
  frames_consumed_on_sink_thread_ = 0;
  silence_frames_written_after_eos_on_sink_thread_ = 0;

  if (first_input_written_) {
    decoder_->Reset();
    decoder_sample_rate_ = nullopt;
    first_input_written_ = false;
  }

  CancelPendingJobs();

#if SB_PLAYER_FILTER_ENABLE_STATE_CHECK
  Schedule(std::bind(&AudioRendererImpl::CheckAudioSinkStatus, this),
           kCheckAudioSinkStatusInterval);
#endif  // SB_PLAYER_FILTER_ENABLE_STATE_CHECK
}

SbTime AudioRendererImpl::GetCurrentMediaTime(bool* is_playing,
                                              bool* is_eos_played,
                                              bool* is_underflow,
                                              double* playback_rate) {
  SB_DCHECK(is_playing);
  SB_DCHECK(is_eos_played);
  SB_DCHECK(is_underflow);
  SB_DCHECK(playback_rate);

  SbTime media_time = 0;
  SbTimeMonotonic now = -1;
  SbTimeMonotonic elasped_since_last_set = 0;
  int64_t frames_played = 0;
  int samples_per_second = 1;

  {
    ScopedLock scoped_lock(mutex_);

    *is_playing = !paused_ && !seeking_;
    *is_eos_played = IsEndOfStreamPlayed_Locked();
    *is_underflow = underflow_;

    if (seeking_ || !decoder_sample_rate_) {
      *playback_rate = playback_rate_;
      return seeking_to_time_;
    }

    if (frames_consumed_by_sink_since_last_get_current_time_ > 0) {
      audio_frame_tracker_.RecordPlayedFrames(
          frames_consumed_by_sink_since_last_get_current_time_);
#if SB_LOG_MEDIA_TIME_STATS
      total_frames_consumed_ +=
          frames_consumed_by_sink_since_last_get_current_time_;
#endif  // SB_LOG_MEDIA_TIME_STATS
      frames_consumed_by_sink_since_last_get_current_time_ = 0;
    }

    // When the audio sink is transitioning from pause to play, it may come with
    // a long delay.  So ensure that ConsumeFrames() is called after Play()
    // before taking elapsed time into account.
    if (!paused_ && playback_rate_ > 0.f && consume_frames_called_) {
      now = SbTimeGetMonotonicNow();
      elasped_since_last_set = now - frames_consumed_set_at_;
    }
    samples_per_second = *decoder_sample_rate_;
    int64_t elapsed_frames =
        elasped_since_last_set * samples_per_second / kSbTimeSecond;
    frames_played =
        audio_frame_tracker_.GetFutureFramesPlayedAdjustedToPlaybackRate(
            elapsed_frames, playback_rate);
    media_time =
        seeking_to_time_ + frames_played * kSbTimeSecond / samples_per_second;
    if (media_time < last_media_time_) {
#if SB_LOG_MEDIA_TIME_STATS
      SB_LOG(WARNING) << "Audio time runs backwards from " << last_media_time_
                      << " to " << media_time;
#endif  // SB_LOG_MEDIA_TIME_STATS
      media_time = last_media_time_;
    }
    last_media_time_ = media_time;
  }

#if SB_LOG_MEDIA_TIME_STATS
  if (system_and_media_time_offset_ < 0 && frames_played > 0) {
    system_and_media_time_offset_ = now - media_time;
  }
  if (system_and_media_time_offset_ > 0) {
    SbTime offset = now - media_time;
    SbTime drift = offset - system_and_media_time_offset_;
    min_drift_ = std::min(drift, min_drift_);
    max_drift_ = std::max(drift, max_drift_);
    SB_LOG(ERROR) << "Media time stats: (" << now << "-"
                  << frames_consumed_set_at_ << "=" << elasped_since_last_set
                  << ") + " << total_frames_consumed_ << " => " << frames_played
                  << " => " << media_time << "  drift: " << drift << "/ ("
                  << min_drift_ << ", " << max_drift_
                  << ") "
                  // How long the audio frames left in sink can be played.
                  << (total_frames_sent_to_sink_ -
                      total_frames_consumed_by_sink_) *
                         kSbTimeSecond / samples_per_second;
  }
#endif  // SB_LOG_MEDIA_TIME_STATS

  return media_time;
}

void AudioRendererImpl::GetSourceStatus(int* frames_in_buffer,
                                        int* offset_in_frames,
                                        bool* is_playing,
                                        bool* is_eos_reached) {
#if SB_PLAYER_FILTER_ENABLE_STATE_CHECK
  sink_callbacks_since_last_check_.increment();
#endif  // SB_PLAYER_FILTER_ENABLE_STATE_CHECK

  {
    ScopedTryLock lock(mutex_);
    if (lock.is_locked()) {
      UpdateVariablesOnSinkThread_Locked(
          frames_consumed_set_at_on_sink_thread_);
    }
  }

  *is_eos_reached = is_eos_reached_on_sink_thread_;
  *is_playing = is_playing_on_sink_thread_;

  if (*is_playing) {
    *frames_in_buffer =
        frames_in_buffer_on_sink_thread_ - frames_consumed_on_sink_thread_;
    *offset_in_frames =
        (offset_in_frames_on_sink_thread_ + frames_consumed_on_sink_thread_) %
        max_cached_frames_;
  } else {
    *frames_in_buffer = *offset_in_frames = 0;
  }

  if (*is_eos_reached && *frames_in_buffer < max_cached_frames_) {
    // Fill silence frames on EOS to ensure keep the audio sink playing.
    if (max_cached_frames_ - *frames_in_buffer >
        silence_frames_written_after_eos_on_sink_thread_) {
      auto silence_frames_to_write =
          max_cached_frames_ - *frames_in_buffer -
          silence_frames_written_after_eos_on_sink_thread_;
      auto start_offset = (*offset_in_frames + *frames_in_buffer +
                           silence_frames_written_after_eos_on_sink_thread_) %
                          max_cached_frames_;

      if (silence_frames_to_write <= max_cached_frames_ - start_offset) {
        memset(frame_buffer_.data() + start_offset * bytes_per_frame_, 0,
                    silence_frames_to_write * bytes_per_frame_);
      } else {
        memset(frame_buffer_.data() + start_offset * bytes_per_frame_, 0,
                    (max_cached_frames_ - start_offset) * bytes_per_frame_);
        memset(
            frame_buffer_.data(), 0,
            (silence_frames_to_write - max_cached_frames_ + start_offset) *
                bytes_per_frame_);
      }
      silence_frames_written_after_eos_on_sink_thread_ +=
          silence_frames_to_write;
    }
    *frames_in_buffer = max_cached_frames_;
  }
}

void AudioRendererImpl::ConsumeFrames(int frames_consumed,
                                      SbTime frames_consumed_at) {
#if SB_PLAYER_FILTER_ENABLE_STATE_CHECK
  sink_callbacks_since_last_check_.increment();
#endif  // SB_PLAYER_FILTER_ENABLE_STATE_CHECK

  // Note that occasionally thread context switch may cause that the time
  // recorded here is several milliseconds later than the time |frames_consumed|
  // is recorded.  This causes the audio time to drift as much as the difference
  // between the two times.
  // This is usually not a huge issue as:
  // 1. It happens rarely.
  // 2. It doesn't accumulate.
  // 3. It doesn't affect frame presenting even with a 60fps video.
  // However, if this ever becomes a problem, we can smooth it out over multiple
  // ConsumeFrames() calls.

  ScopedTryLock lock(mutex_);
  if (lock.is_locked()) {
    frames_consumed_on_sink_thread_ += frames_consumed;

    UpdateVariablesOnSinkThread_Locked(frames_consumed_at);
  } else {
    frames_consumed_on_sink_thread_ += frames_consumed;
    frames_consumed_set_at_on_sink_thread_ = frames_consumed_at;
  }
}

void AudioRendererImpl::OnError(bool capability_changed,
                                const std::string& error_message) {
  SB_DCHECK(error_cb_);
  if (capability_changed) {
    error_cb_(kSbPlayerErrorCapabilityChanged, error_message);
  } else {
    // Send |kSbPlayerErrorDecode| on fatal audio sink error.  The error code
    // will be mapped into MediaError eventually, and there is no corresponding
    // error code in MediaError for audio sink error anyway.
    error_cb_(kSbPlayerErrorDecode, error_message);
  }
}

void AudioRendererImpl::UpdateVariablesOnSinkThread_Locked(
    SbTime system_time_on_consume_frames) {
  mutex_.DCheckAcquired();

  if (frames_consumed_on_sink_thread_ > 0) {
    SB_DCHECK(total_frames_consumed_by_sink_ +
                  frames_consumed_on_sink_thread_ <=
              total_frames_sent_to_sink_ +
                  silence_frames_written_after_eos_on_sink_thread_);
    auto non_silence_frames_consumed =
        std::min(total_frames_sent_to_sink_ - total_frames_consumed_by_sink_,
                 frames_consumed_on_sink_thread_);
    total_frames_consumed_by_sink_ += non_silence_frames_consumed;
    frames_consumed_by_sink_since_last_get_current_time_ +=
        non_silence_frames_consumed;
    if (non_silence_frames_consumed != 0) {
      frames_consumed_set_at_ = system_time_on_consume_frames;
    }
    consume_frames_called_ = true;
    frames_consumed_on_sink_thread_ = 0;
  }

  is_eos_reached_on_sink_thread_ = eos_state_ >= kEOSSentToSink;
  frames_in_buffer_on_sink_thread_ = static_cast<int>(
      total_frames_sent_to_sink_ - total_frames_consumed_by_sink_);
  underflow_ |=
      frames_in_buffer_on_sink_thread_ < kFramesInBufferBeginUnderflow;
  if (is_eos_reached_on_sink_thread_ ||
      frames_in_buffer_on_sink_thread_ >= buffered_frames_to_start_) {
    underflow_ = false;
  }
  is_playing_on_sink_thread_ = !paused_ && !seeking_ && !underflow_;
  offset_in_frames_on_sink_thread_ =
      total_frames_consumed_by_sink_ % max_cached_frames_;

  if (IsEndOfStreamPlayed_Locked() && !ended_cb_called_) {
    ended_cb_called_ = true;
    Schedule(ended_cb_);
  }
}

void AudioRendererImpl::OnFirstOutput(
    const SbMediaAudioSampleType decoded_sample_type,
    const SbMediaAudioFrameStorageType decoded_storage_type,
    const int decoded_sample_rate) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(!decoder_sample_rate_);
  decoder_sample_rate_ = decoded_sample_rate;
  int destination_sample_rate =
      audio_renderer_sink_->GetNearestSupportedSampleFrequency(
          *decoder_sample_rate_);
  time_stretcher_.Initialize(kSbMediaAudioSampleTypeFloat32, channels_,
                             destination_sample_rate);

  buffered_frames_to_start_ = std::min(
      destination_sample_rate / 5, max_cached_frames_ - min_frames_per_append_);

  if (decoded_sample_rate != destination_sample_rate ||
      decoded_sample_type != sink_sample_type_ ||
      decoded_storage_type != kSbMediaAudioFrameStorageTypeInterleaved) {
    resampler_ = AudioResampler::Create(
        decoded_sample_type, decoded_storage_type, decoded_sample_rate,
        sink_sample_type_, kSbMediaAudioFrameStorageTypeInterleaved,
        destination_sample_rate, channels_);
    SB_DCHECK(resampler_);
  } else {
    resampler_.reset(new IdentityAudioResampler);
  }

  // TODO: Support planar only audio sink.
  audio_renderer_sink_->Start(
      seeking_to_time_, channels_, destination_sample_rate, sink_sample_type_,
      kSbMediaAudioFrameStorageTypeInterleaved,
      reinterpret_cast<SbAudioSinkFrameBuffers>(frame_buffers_),
      max_cached_frames_, this);
  if (!audio_renderer_sink_->HasStarted()) {
    SB_LOG(ERROR) << "Failed to start audio sink.";
    error_cb_(kSbPlayerErrorDecode, "failed to start audio sink");
  }
}

bool AudioRendererImpl::IsEndOfStreamPlayed_Locked() const {
  mutex_.DCheckAcquired();
  return eos_state_ >= kEOSSentToSink &&
         total_frames_sent_to_sink_ == total_frames_consumed_by_sink_;
}

void AudioRendererImpl::OnDecoderConsumed() {
  SB_DCHECK(BelongsToCurrentThread());

  // TODO: Unify EOS and non EOS request once WriteEndOfStream() depends on
  // CanAcceptMoreData().
  if (eos_state_ == kEOSNotReceived) {
    SB_DCHECK(!can_accept_more_data_);

    can_accept_more_data_ = true;
  }
}

void AudioRendererImpl::OnDecoderOutput() {
  SB_DCHECK(BelongsToCurrentThread());

  ++pending_decoder_outputs_;

  if (process_audio_data_job_token_.is_valid()) {
    RemoveJobByToken(process_audio_data_job_token_);
    process_audio_data_job_token_.ResetToInvalid();
  }

  ProcessAudioData();
}

void AudioRendererImpl::ProcessAudioData() {
  SB_DCHECK(BelongsToCurrentThread());

  process_audio_data_job_token_.ResetToInvalid();

  // Loop until no audio is appended, i.e. AppendAudioToFrameBuffer() returns
  // false.
  bool is_frame_buffer_full = false;
  if (audio_renderer_sink_->HasStarted()) {
    while (AppendAudioToFrameBuffer(&is_frame_buffer_full)) {
    }
  }

  while (pending_decoder_outputs_ > 0) {
    if (audio_renderer_sink_->HasStarted() && time_stretcher_.IsQueueFull()) {
      // There is no room to do any further processing, schedule the function
      // again for a later time.  The delay time is 1/4 of the buffer size.
      const SbTimeMonotonic delay =
          max_cached_frames_ * kSbTimeSecond / *decoder_sample_rate_ / 4;
      process_audio_data_job_token_ = Schedule(process_audio_data_job_, delay);
      return;
    }

    scoped_refptr<DecodedAudio> resampled_audio;
    int decoded_audio_sample_rate;
    scoped_refptr<DecodedAudio> decoded_audio =
        decoder_->Read(&decoded_audio_sample_rate);
    SB_DCHECK(decoded_audio);
    if (!audio_renderer_sink_->HasStarted()) {
      if (!decoded_audio->is_end_of_stream()) {
        decoded_audio->AdjustForSeekTime(decoded_audio_sample_rate,
                                         seeking_to_time_);
      }
      OnFirstOutput(decoded_audio->sample_type(), decoded_audio->storage_type(),
                    decoded_audio_sample_rate);
    }
    SB_DCHECK(resampler_);

    --pending_decoder_outputs_;
    if (!decoded_audio) {
      continue;
    }

    if (decoded_audio->is_end_of_stream()) {
      SB_DCHECK(eos_state_ == kEOSWrittenToDecoder) << eos_state_;
      {
        ScopedLock lock(mutex_);
        eos_state_ = kEOSDecoded;
        if (seeking_) {
          seeking_ = false;
          Schedule(prerolled_cb_);
        }
      }

      resampled_audio = resampler_->WriteEndOfStream();
    } else {
      // Discard any audio data before the seeking target.
      if (seeking_ && decoded_audio->timestamp() < seeking_to_time_) {
        continue;
      }

      resampled_audio = resampler_->Resample(decoded_audio);
    }

    if (resampled_audio && resampled_audio->size() > 0) {
      // |time_stretcher_| only support kSbMediaAudioSampleTypeFloat32 and
      // kSbMediaAudioFrameStorageTypeInterleaved.
      resampled_audio->SwitchFormatTo(kSbMediaAudioSampleTypeFloat32,
                                      kSbMediaAudioFrameStorageTypeInterleaved);
      time_stretcher_.EnqueueBuffer(resampled_audio);
    }

    // Loop until no audio is appended, i.e. AppendAudioToFrameBuffer() returns
    // false.
    while (AppendAudioToFrameBuffer(&is_frame_buffer_full)) {
    }
  }

  if (seeking_ || playback_rate_ == 0.0) {
    process_audio_data_job_token_ =
        Schedule(process_audio_data_job_, 5 * kSbTimeMillisecond);
    return;
  }

  if (is_frame_buffer_full) {
    // There are still audio data not appended so schedule a callback later.
    SbTimeMonotonic delay = 0;
    int64_t frames_in_buffer =
        total_frames_sent_to_sink_ - total_frames_consumed_by_sink_;
    if (max_cached_frames_ - frames_in_buffer < max_cached_frames_ / 4) {
      int frames_to_delay = static_cast<int>(
          max_cached_frames_ / 4 - (max_cached_frames_ - frames_in_buffer));
      delay = frames_to_delay * kSbTimeSecond / *decoder_sample_rate_;
    }
    process_audio_data_job_token_ = Schedule(process_audio_data_job_, delay);
  }
}

bool AudioRendererImpl::AppendAudioToFrameBuffer(bool* is_frame_buffer_full) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(is_frame_buffer_full);

  *is_frame_buffer_full = false;

  if (time_stretcher_.IsQueueFull()) {
    ScopedLock lock(mutex_);
    if (seeking_) {
      seeking_ = false;
      Schedule(prerolled_cb_);
    }
  }

  if (seeking_ || playback_rate_ == 0.0) {
    return false;
  }

  int frames_in_buffer = static_cast<int>(total_frames_sent_to_sink_ -
                                          total_frames_consumed_by_sink_);

  if (max_cached_frames_ - frames_in_buffer < min_frames_per_append_) {
    *is_frame_buffer_full = true;
    return false;
  }

  int offset_to_append = total_frames_sent_to_sink_ % max_cached_frames_;

  scoped_refptr<DecodedAudio> decoded_audio = time_stretcher_.Read(
      max_cached_frames_ - frames_in_buffer, playback_rate_);
  SB_DCHECK(decoded_audio);

  {
    ScopedLock lock(mutex_);
    if (decoded_audio->frames() == 0 && eos_state_ == kEOSDecoded) {
      eos_state_ = kEOSSentToSink;
    }
    audio_frame_tracker_.AddFrames(decoded_audio->frames(), playback_rate_);
  }

  // |time_stretcher_| only support kSbMediaAudioSampleTypeFloat32 and
  // kSbMediaAudioFrameStorageTypeInterleaved.
  decoded_audio->SwitchFormatTo(sink_sample_type_,
                                kSbMediaAudioFrameStorageTypeInterleaved);
  const uint8_t* source_buffer = decoded_audio->buffer();
  int frames_to_append = decoded_audio->frames();
  int frames_appended = 0;

  if (frames_to_append > max_cached_frames_ - offset_to_append) {
    memcpy(&frame_buffer_[offset_to_append * bytes_per_frame_],
                 source_buffer,
                 (max_cached_frames_ - offset_to_append) * bytes_per_frame_);
    source_buffer += (max_cached_frames_ - offset_to_append) * bytes_per_frame_;
    frames_to_append -= max_cached_frames_ - offset_to_append;
    frames_appended += max_cached_frames_ - offset_to_append;
    offset_to_append = 0;
  }

  memcpy(&frame_buffer_[offset_to_append * bytes_per_frame_],
               source_buffer, frames_to_append * bytes_per_frame_);
  frames_appended += frames_to_append;

  total_frames_sent_to_sink_ += frames_appended;

  return frames_appended > 0;
}

#if SB_PLAYER_FILTER_ENABLE_STATE_CHECK
void AudioRendererImpl::CheckAudioSinkStatus() {
  SB_DCHECK(BelongsToCurrentThread());

  // Check if sink callbacks are called too frequently.
  if (sink_callbacks_since_last_check_.load() > kMaxSinkCallbacksBetweenCheck) {
    SB_LOG(WARNING) << "Sink callback has been called for "
                    << sink_callbacks_since_last_check_.load()
                    << " time since last check, which is too frequently.";
  }

  auto sink_callbacks_since_last_check =
      sink_callbacks_since_last_check_.exchange(0);

  if (paused_ || playback_rate_ == 0.0) {
    return;
  }

  // Check if sink has updated.
  SbTimeMonotonic elapsed = SbTimeGetMonotonicNow() - frames_consumed_set_at_;
  if (elapsed > kCheckAudioSinkStatusInterval) {
    ScopedLock lock(mutex_);
    SB_DLOG(WARNING) << "|frames_consumed_| has not been updated for "
                     << elapsed / kSbTimeSecond << " seconds, with "
                     << total_frames_sent_to_sink_ -
                            total_frames_consumed_by_sink_
                     << " frames in sink, " << (underflow_ ? "underflow, " : "")
                     << sink_callbacks_since_last_check
                     << " callbacks since last check.";
  }
  Schedule(std::bind(&AudioRendererImpl::CheckAudioSinkStatus, this),
           kCheckAudioSinkStatusInterval);
}
#endif  // SB_PLAYER_FILTER_ENABLE_STATE_CHECK

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
