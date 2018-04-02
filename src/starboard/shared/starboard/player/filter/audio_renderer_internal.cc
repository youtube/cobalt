// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "starboard/shared/starboard/player/filter/audio_renderer_internal.h"

#include <algorithm>

#include "starboard/memory.h"
#include "starboard/shared/starboard/media/media_util.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

namespace {

// This class works only when the input format and output format are the same.
// It allows for a simplified AudioRenderer implementation by always using a
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

// AudioRenderer uses AudioTimeStretcher internally to adjust to playback rate
// and AudioTimeStretcher can only process float32 samples.  So we try to use
// kSbMediaAudioSampleTypeFloat32 and only use
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

AudioRenderer::AudioRenderer(scoped_ptr<AudioDecoder> decoder,
                             scoped_ptr<AudioRendererSink> audio_renderer_sink,
                             const SbMediaAudioHeader& audio_header,
                             int max_cached_frames,
                             int max_frames_per_append)
    : max_cached_frames_(max_cached_frames),
      max_frames_per_append_(max_frames_per_append),
      eos_state_(kEOSNotReceived),
      channels_(audio_header.number_of_channels),
      sink_sample_type_(GetSinkAudioSampleType(audio_renderer_sink.get())),
      bytes_per_frame_(media::GetBytesPerSample(sink_sample_type_) * channels_),
      playback_rate_(1.0),
      paused_(true),
      consume_frames_called_(false),
      seeking_(false),
      seeking_to_time_(0),
      last_time_(0),
      frame_buffer_(max_cached_frames_ * bytes_per_frame_),
      frames_sent_to_sink_(0),
      pending_decoder_outputs_(0),
      frames_consumed_by_sink_(0),
      frames_consumed_set_at_(SbTimeGetMonotonicNow()),
      decoder_(decoder.Pass()),
      can_accept_more_data_(true),
      process_audio_data_job_(
          std::bind(&AudioRenderer::ProcessAudioData, this)),
      first_input_written_(false),
      audio_renderer_sink_(audio_renderer_sink.Pass()) {
  SB_DCHECK(decoder_ != NULL);
  SB_DCHECK(max_frames_per_append_ > 0);
  SB_DCHECK(max_cached_frames_ >= max_frames_per_append_ * 2);

  frame_buffers_[0] = &frame_buffer_[0];

#if defined(NDEBUG)
  const bool kLogFramesConsumed = false;
#else
  const bool kLogFramesConsumed = true;
#endif
  if (kLogFramesConsumed) {
    log_frames_consumed_closure_ =
        std::bind(&AudioRenderer::LogFramesConsumed, this);
    Schedule(log_frames_consumed_closure_, kSbTimeSecond);
  }
}

AudioRenderer::~AudioRenderer() {
  SB_DCHECK(BelongsToCurrentThread());
}

void AudioRenderer::Initialize(const AudioDecoder::ErrorCB& error_cb) {
  decoder_->Initialize(std::bind(&AudioRenderer::OnDecoderOutput, this),
                       error_cb);
}

void AudioRenderer::WriteSample(
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
                   std::bind(&AudioRenderer::OnDecoderConsumed, this));
  first_input_written_ = true;
}

void AudioRenderer::WriteEndOfStream() {
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

void AudioRenderer::SetVolume(double volume) {
  SB_DCHECK(BelongsToCurrentThread());
  audio_renderer_sink_->SetVolume(volume);
}

bool AudioRenderer::IsEndOfStreamWritten() const {
  SB_DCHECK(BelongsToCurrentThread());
  return eos_state_ >= kEOSWrittenToDecoder;
}

bool AudioRenderer::IsEndOfStreamPlayed() const {
  ScopedLock lock(mutex_);
  return IsEndOfStreamPlayed_Locked();
}

bool AudioRenderer::CanAcceptMoreData() const {
  SB_DCHECK(BelongsToCurrentThread());
  return eos_state_ == kEOSNotReceived && can_accept_more_data_ &&
         (!decoder_sample_rate_ || !time_stretcher_.IsQueueFull());
}

bool AudioRenderer::IsSeekingInProgress() const {
  SB_DCHECK(BelongsToCurrentThread());
  return seeking_;
}

void AudioRenderer::Play() {
  SB_DCHECK(BelongsToCurrentThread());

  ScopedLock lock(mutex_);
  paused_ = false;
  consume_frames_called_ = false;
}

void AudioRenderer::Pause() {
  SB_DCHECK(BelongsToCurrentThread());

  ScopedLock lock(mutex_);
  paused_ = true;
}

void AudioRenderer::SetPlaybackRate(double playback_rate) {
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

void AudioRenderer::Seek(SbTime seek_to_time) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(seek_to_time >= 0);

  audio_renderer_sink_->Stop();

  {
    // Set the following states under a lock first to ensure that from now on
    // GetCurrentMediaTime() returns |seeking_to_time_|.
    ScopedLock scoped_lock(mutex_);
    eos_state_ = kEOSNotReceived;
    seeking_to_time_ = std::max<SbTime>(seek_to_time, 0);
    last_time_ = seek_to_time;
    seeking_ = true;
  }

  // Now the sink is stopped and the callbacks will no longer be called, so the
  // following modifications are safe without lock.
  if (resampler_) {
    resampler_.reset();
    time_stretcher_.FlushBuffers();
  }

  frames_sent_to_sink_ = 0;
  frames_consumed_by_sink_ = 0;
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

  if (first_input_written_) {
    decoder_->Reset();
    decoder_sample_rate_ = nullopt;
    first_input_written_ = false;
  }

  CancelPendingJobs();

  if (log_frames_consumed_closure_) {
    Schedule(log_frames_consumed_closure_, kSbTimeSecond);
  }
}

SbTime AudioRenderer::GetCurrentMediaTime(bool* is_playing,
                                          bool* is_eos_played) {
  SB_DCHECK(is_playing);
  SB_DCHECK(is_eos_played);

  SbTime media_sb_time = 0;
  SbTimeMonotonic now = -1;
  SbTimeMonotonic elasped_since_last_set = 0;
  int64_t frames_played = 0;
  int samples_per_second = 1;

  {
    ScopedLock scoped_lock(mutex_);

    *is_playing = !paused_ && !seeking_;
    *is_eos_played = IsEndOfStreamPlayed_Locked();

    if (seeking_ || !decoder_sample_rate_) {
      return seeking_to_time_;
    }

    if (frames_consumed_by_sink_since_last_get_current_time_ > 0) {
      audio_frame_tracker_.RecordPlayedFrames(
          frames_consumed_by_sink_since_last_get_current_time_);
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
            elapsed_frames);
    media_sb_time =
        seeking_to_time_ + frames_played * kSbTimeSecond / samples_per_second;
    if (media_sb_time < last_time_) {
      SB_DLOG(WARNING) << "Audio time runs backwards from " << last_time_
                       << " to " << media_sb_time;
      media_sb_time = last_time_;
    }
    last_time_ = media_sb_time;
  }

#if SB_LOG_MEDIA_TIME_STATS
  if (system_and_media_time_offset_ < 0 && frames_played > 0) {
    system_and_media_time_offset_ = now - media_sb_time;
  }
  if (system_and_media_time_offset_ > 0) {
    SbTime offset = now - media_sb_time;
    SbTime diff = std::abs(offset - system_and_media_time_offset_);
    max_offset_difference_ = std::max(diff, max_offset_difference_);
    SB_LOG(ERROR) << "Media time stats: (" << now << "-"
                  << frames_consumed_set_at_ << "=" << elasped_since_last_set
                  << ") => " << frames_played << " => " << media_sb_time
                  << "  diff: " << diff << "/" << max_offset_difference_;
  }
#endif  // SB_LOG_MEDIA_TIME_STATS

  return media_sb_time;
}

void AudioRenderer::GetSourceStatus(int* frames_in_buffer,
                                    int* offset_in_frames,
                                    bool* is_playing,
                                    bool* is_eos_reached) {
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
}

void AudioRenderer::ConsumeFrames(int frames_consumed) {
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
  auto system_time = SbTimeGetMonotonicNow();

  ScopedTryLock lock(mutex_);
  if (lock.is_locked()) {
    frames_consumed_on_sink_thread_ += frames_consumed;

    UpdateVariablesOnSinkThread_Locked(system_time);
  } else {
    frames_consumed_on_sink_thread_ += frames_consumed;
    frames_consumed_set_at_on_sink_thread_ = system_time;
  }
}

void AudioRenderer::UpdateVariablesOnSinkThread_Locked(
    SbTime system_time_on_consume_frames) {
  mutex_.DCheckAcquired();

  if (frames_consumed_on_sink_thread_ > 0) {
    frames_consumed_by_sink_ += frames_consumed_on_sink_thread_;
    SB_DCHECK(frames_consumed_by_sink_ <= frames_sent_to_sink_);
    frames_consumed_by_sink_since_last_get_current_time_ +=
        frames_consumed_on_sink_thread_;
    frames_consumed_set_at_ = system_time_on_consume_frames;
    consume_frames_called_ = true;
    frames_consumed_on_sink_thread_ = 0;
  }

  is_eos_reached_on_sink_thread_ = eos_state_ >= kEOSSentToSink;
  is_playing_on_sink_thread_ = !paused_ && !seeking_;
  frames_in_buffer_on_sink_thread_ =
      static_cast<int>(frames_sent_to_sink_ - frames_consumed_by_sink_);
  offset_in_frames_on_sink_thread_ =
      frames_consumed_by_sink_ % max_cached_frames_;
}

void AudioRenderer::OnFirstOutput() {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(!decoder_sample_rate_);
  decoder_sample_rate_ = decoder_->GetSamplesPerSecond();
  int destination_sample_rate =
      audio_renderer_sink_->GetNearestSupportedSampleFrequency(
          *decoder_sample_rate_);
  time_stretcher_.Initialize(channels_, destination_sample_rate);

  SbMediaAudioSampleType source_sample_type = decoder_->GetSampleType();
  SbMediaAudioFrameStorageType source_storage_type = decoder_->GetStorageType();

  // AudioTimeStretcher only supports interleaved float32 samples.
  if (*decoder_sample_rate_ != destination_sample_rate ||
      source_sample_type != kSbMediaAudioSampleTypeFloat32 ||
      source_storage_type != kSbMediaAudioFrameStorageTypeInterleaved) {
    resampler_ = AudioResampler::Create(
        decoder_->GetSampleType(), decoder_->GetStorageType(),
        *decoder_sample_rate_, kSbMediaAudioSampleTypeFloat32,
        kSbMediaAudioFrameStorageTypeInterleaved, destination_sample_rate,
        channels_);
    SB_DCHECK(resampler_);
  } else {
    resampler_.reset(new IdentityAudioResampler);
  }

  // TODO: Support planar only audio sink.
  audio_renderer_sink_->Start(
      channels_, destination_sample_rate, sink_sample_type_,
      kSbMediaAudioFrameStorageTypeInterleaved,
      reinterpret_cast<SbAudioSinkFrameBuffers>(frame_buffers_),
      max_cached_frames_, this);
  SB_DCHECK(audio_renderer_sink_->HasStarted());
}

void AudioRenderer::LogFramesConsumed() {
  SB_DCHECK(BelongsToCurrentThread());
  SbTimeMonotonic time_since =
      SbTimeGetMonotonicNow() - frames_consumed_set_at_;
  if (time_since > kSbTimeSecond) {
    SB_DLOG(WARNING) << "|frames_consumed_| has not been updated for "
                     << (time_since / kSbTimeSecond) << "."
                     << ((time_since / (kSbTimeSecond / 10)) % 10)
                     << " seconds";
  }
  Schedule(log_frames_consumed_closure_, kSbTimeSecond);
}

bool AudioRenderer::IsEndOfStreamPlayed_Locked() const {
  mutex_.DCheckAcquired();
  return eos_state_ >= kEOSSentToSink &&
         frames_sent_to_sink_ == frames_consumed_by_sink_;
}

void AudioRenderer::OnDecoderConsumed() {
  SB_DCHECK(BelongsToCurrentThread());

  // TODO: Unify EOS and non EOS request once WriteEndOfStream() depends on
  // CanAcceptMoreData().
  if (eos_state_ == kEOSNotReceived) {
    SB_DCHECK(!can_accept_more_data_);

    can_accept_more_data_ = true;
  }
}

void AudioRenderer::OnDecoderOutput() {
  SB_DCHECK(BelongsToCurrentThread());

  ++pending_decoder_outputs_;

  if (process_audio_data_job_token_.is_valid()) {
    RemoveJobByToken(process_audio_data_job_token_);
    process_audio_data_job_token_.ResetToInvalid();
  }

  if (!audio_renderer_sink_->HasStarted()) {
    OnFirstOutput();
  }

  ProcessAudioData();
}

void AudioRenderer::ProcessAudioData() {
  SB_DCHECK(BelongsToCurrentThread());

  process_audio_data_job_token_.ResetToInvalid();

  SB_DCHECK(resampler_);

  // Loop until no audio is appended, i.e. AppendAudioToFrameBuffer() returns
  // false.
  bool is_frame_buffer_full = false;
  while (AppendAudioToFrameBuffer(&is_frame_buffer_full)) {
  }

  while (pending_decoder_outputs_ > 0) {
    if (time_stretcher_.IsQueueFull()) {
      // There is no room to do any further processing, schedule the function
      // again for a later time.  The delay time is 1/4 of the buffer size.
      const SbTimeMonotonic delay =
          max_cached_frames_ * kSbTimeSecond / *decoder_sample_rate_ / 4;
      process_audio_data_job_token_ = Schedule(process_audio_data_job_, delay);
      return;
    }

    scoped_refptr<DecodedAudio> resampled_audio;
    scoped_refptr<DecodedAudio> decoded_audio = decoder_->Read();

    --pending_decoder_outputs_;
    SB_DCHECK(decoded_audio);
    if (!decoded_audio) {
      continue;
    }

    if (decoded_audio->is_end_of_stream()) {
      SB_DCHECK(eos_state_ == kEOSWrittenToDecoder) << eos_state_;
      {
        ScopedLock lock(mutex_);
        eos_state_ = kEOSDecoded;
        seeking_ = false;
      }

      resampled_audio = resampler_->WriteEndOfStream();
    } else {
      // Discard any audio data before the seeking target.
      if (seeking_ && decoded_audio->timestamp() < seeking_to_time_) {
        continue;
      }

      resampled_audio = resampler_->Resample(decoded_audio);
    }

    if (resampled_audio->size() > 0) {
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
    int64_t frames_in_buffer = frames_sent_to_sink_ - frames_consumed_by_sink_;
    if (max_cached_frames_ - frames_in_buffer < max_cached_frames_ / 4) {
      int frames_to_delay = static_cast<int>(
          max_cached_frames_ / 4 - (max_cached_frames_ - frames_in_buffer));
      delay = frames_to_delay * kSbTimeSecond / *decoder_sample_rate_;
    }
    process_audio_data_job_token_ = Schedule(process_audio_data_job_, delay);
  }
}

bool AudioRenderer::AppendAudioToFrameBuffer(bool* is_frame_buffer_full) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(is_frame_buffer_full);

  *is_frame_buffer_full = false;

  if (seeking_ && time_stretcher_.IsQueueFull()) {
    ScopedLock lock(mutex_);
    seeking_ = false;
  }

  if (seeking_ || playback_rate_ == 0.0) {
    return false;
  }

  int frames_in_buffer =
      static_cast<int>(frames_sent_to_sink_ - frames_consumed_by_sink_);

  if (max_cached_frames_ - frames_in_buffer < max_frames_per_append_) {
    *is_frame_buffer_full = true;
    return false;
  }

  int offset_to_append = frames_sent_to_sink_ % max_cached_frames_;

  scoped_refptr<DecodedAudio> decoded_audio =
      time_stretcher_.Read(max_frames_per_append_, playback_rate_);
  SB_DCHECK(decoded_audio);

  {
    ScopedLock lock(mutex_);
    if (decoded_audio->frames() == 0 && eos_state_ == kEOSDecoded) {
      eos_state_ = kEOSSentToSink;
    }
    audio_frame_tracker_.AddFrames(decoded_audio->frames(), playback_rate_);
  }

  // TODO: Support kSbMediaAudioFrameStorageTypePlanar.
  decoded_audio->SwitchFormatTo(sink_sample_type_,
                                kSbMediaAudioFrameStorageTypeInterleaved);
  const uint8_t* source_buffer = decoded_audio->buffer();
  int frames_to_append = decoded_audio->frames();
  int frames_appended = 0;

  if (frames_to_append > max_cached_frames_ - offset_to_append) {
    SbMemoryCopy(&frame_buffer_[offset_to_append * bytes_per_frame_],
                 source_buffer,
                 (max_cached_frames_ - offset_to_append) * bytes_per_frame_);
    source_buffer += (max_cached_frames_ - offset_to_append) * bytes_per_frame_;
    frames_to_append -= max_cached_frames_ - offset_to_append;
    frames_appended += max_cached_frames_ - offset_to_append;
    offset_to_append = 0;
  }

  SbMemoryCopy(&frame_buffer_[offset_to_append * bytes_per_frame_],
               source_buffer, frames_to_append * bytes_per_frame_);
  frames_appended += frames_to_append;

  frames_sent_to_sink_ += frames_appended;

  return frames_appended > 0;
}

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
