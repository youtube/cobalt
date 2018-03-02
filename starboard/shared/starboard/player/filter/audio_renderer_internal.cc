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
// kSbMediaAudioSampleTypeFloat32 and only use kSbMediaAudioSampleTypeInt16 when
// float32 is not supported.  To use kSbMediaAudioSampleTypeFloat32 will cause
// an extra conversion from float32 to int16 before the samples are sent to the
// audio sink.
SbMediaAudioSampleType GetSinkAudioSampleType(
    AudioRendererSink* audio_renderer_sink) {
  return audio_renderer_sink->IsAudioSampleTypeSupported(
             kSbMediaAudioSampleTypeFloat32)
             ? kSbMediaAudioSampleTypeFloat32
             : kSbMediaAudioSampleTypeInt16;
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
      seeking_to_pts_(0),
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

  if (eos_state_.load() >= kEOSWrittenToDecoder) {
    SB_LOG(ERROR) << "Appending audio sample at " << input_buffer->pts()
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

  if (eos_state_.load() >= kEOSWrittenToDecoder) {
    SB_LOG(ERROR) << "Try to write EOS after EOS is reached";
    return;
  }

  decoder_->WriteEndOfStream();

  eos_state_.store(kEOSWrittenToDecoder);
  first_input_written_ = true;
}

void AudioRenderer::SetVolume(double volume) {
  SB_DCHECK(BelongsToCurrentThread());
  audio_renderer_sink_->SetVolume(volume);
}

bool AudioRenderer::IsEndOfStreamPlayed() const {
  return eos_state_.load() >= kEOSSentToSink &&
         frames_sent_to_sink_.load() == frames_consumed_by_sink_.load();
}

bool AudioRenderer::CanAcceptMoreData() const {
  SB_DCHECK(BelongsToCurrentThread());
  return eos_state_.load() == kEOSNotReceived && can_accept_more_data_ &&
         (!decoder_sample_rate_ || !time_stretcher_.IsQueueFull());
}

bool AudioRenderer::IsSeekingInProgress() const {
  SB_DCHECK(BelongsToCurrentThread());
  return seeking_.load();
}

void AudioRenderer::Play() {
  SB_DCHECK(BelongsToCurrentThread());

  paused_.store(false);
  consume_frames_called_.store(false);
}

void AudioRenderer::Pause() {
  SB_DCHECK(BelongsToCurrentThread());

  paused_.store(true);
}

void AudioRenderer::SetPlaybackRate(double playback_rate) {
  SB_DCHECK(BelongsToCurrentThread());

  if (playback_rate_.load() == 0.f && playback_rate > 0.f) {
    consume_frames_called_.store(false);
  }

  playback_rate_.store(playback_rate);

  audio_renderer_sink_->SetPlaybackRate(playback_rate_.load() > 0.0 ? 1.0
                                                                    : 0.0);
  if (audio_renderer_sink_->HasStarted()) {
    // TODO: Remove SetPlaybackRate() support from audio sink as it only need to
    // support play/pause.
    if (playback_rate_.load() > 0.0) {
      if (process_audio_data_job_token_.is_valid()) {
        RemoveJobByToken(process_audio_data_job_token_);
        process_audio_data_job_token_.ResetToInvalid();
      }
      ProcessAudioData();
    }
  }
}

void AudioRenderer::Seek(SbMediaTime seek_to_pts) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(seek_to_pts >= 0);

  audio_renderer_sink_->Stop();

  // Now the sink is stopped and the callbacks will no longer be called, so the
  // following modifications are safe without lock.
  if (resampler_) {
    resampler_.reset();
    time_stretcher_.FlushBuffers();
  }

  eos_state_.store(kEOSNotReceived);
  seeking_to_pts_ = std::max<SbMediaTime>(seek_to_pts, 0);
  seeking_.store(true);
  frames_sent_to_sink_.store(0);
  frames_consumed_by_sink_.store(0);
  frames_consumed_by_sink_since_last_get_current_time_.store(0);
  pending_decoder_outputs_ = 0;
  audio_frame_tracker_.Reset();
  frames_consumed_set_at_.store(SbTimeGetMonotonicNow());
  can_accept_more_data_ = true;
  process_audio_data_job_token_.ResetToInvalid();

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

SbMediaTime AudioRenderer::GetCurrentMediaTime(bool* is_playing,
                                               bool* is_eos_played) {
  SB_DCHECK(is_playing);
  SB_DCHECK(is_eos_played);

  *is_playing = !paused_.load() && !seeking_.load();
  *is_eos_played = IsEndOfStreamPlayed();

  if (seeking_.load() || !decoder_sample_rate_) {
    return seeking_to_pts_;
  }

  audio_frame_tracker_.RecordPlayedFrames(
      frames_consumed_by_sink_since_last_get_current_time_.exchange(0));

  SbTimeMonotonic elasped_since_last_set = 0;
  // When the audio sink is transitioning from pause to play, it may come with a
  // long delay.  So ensure that ConsumeFrames() is called after Play() before
  // taking elapsed time into account.
  if (!paused_.load() && playback_rate_.load() > 0.f &&
      consume_frames_called_.load()) {
    elasped_since_last_set =
        SbTimeGetMonotonicNow() - frames_consumed_set_at_.load();
  }
  int samples_per_second = *decoder_sample_rate_;
  int64_t elapsed_frames =
      elasped_since_last_set * samples_per_second / kSbTimeSecond;
  int64_t frames_played =
      audio_frame_tracker_.GetFutureFramesPlayedAdjustedToPlaybackRate(
          elapsed_frames);

  return seeking_to_pts_ +
         frames_played * kSbMediaTimeSecond / samples_per_second;
}

void AudioRenderer::GetSourceStatus(int* frames_in_buffer,
                                    int* offset_in_frames,
                                    bool* is_playing,
                                    bool* is_eos_reached) {
  *is_eos_reached = eos_state_.load() >= kEOSSentToSink;

  *is_playing = !paused_.load() && !seeking_.load();

  if (*is_playing) {
    *frames_in_buffer = static_cast<int>(frames_sent_to_sink_.load() -
                                         frames_consumed_by_sink_.load());
    *offset_in_frames = frames_consumed_by_sink_.load() % max_cached_frames_;
  } else {
    *frames_in_buffer = *offset_in_frames = 0;
  }
}

void AudioRenderer::ConsumeFrames(int frames_consumed) {
  frames_consumed_by_sink_.fetch_add(frames_consumed);
  SB_DCHECK(frames_consumed_by_sink_.load() <= frames_sent_to_sink_.load());
  frames_consumed_by_sink_since_last_get_current_time_.fetch_add(
      frames_consumed);
  frames_consumed_set_at_.store(SbTimeGetMonotonicNow());
  consume_frames_called_.store(true);
}

void AudioRenderer::OnFirstOutput() {
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
  SbTimeMonotonic time_since =
      SbTimeGetMonotonicNow() - frames_consumed_set_at_.load();
  if (time_since > kSbTimeSecond) {
    SB_DLOG(WARNING) << "|frames_consumed_| has not been updated for "
                     << (time_since / kSbTimeSecond) << "."
                     << ((time_since / (kSbTimeSecond / 10)) % 10)
                     << " seconds";
  }
  Schedule(log_frames_consumed_closure_, kSbTimeSecond);
}

void AudioRenderer::OnDecoderConsumed() {
  SB_DCHECK(BelongsToCurrentThread());

  // TODO: Unify EOS and non EOS request once WriteEndOfStream() depends on
  // CanAcceptMoreData().
  if (eos_state_.load() == kEOSNotReceived) {
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
      SB_DCHECK(eos_state_.load() == kEOSWrittenToDecoder) << eos_state_.load();
      eos_state_.store(kEOSDecoded);
      seeking_.store(false);

      resampled_audio = resampler_->WriteEndOfStream();
    } else {
      // Discard any audio data before the seeking target.
      if (seeking_.load() && decoded_audio->pts() < seeking_to_pts_) {
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

  if (seeking_.load() || playback_rate_.load() == 0.0) {
    process_audio_data_job_token_ =
        Schedule(process_audio_data_job_, 5 * kSbTimeMillisecond);
    return;
  }

  if (is_frame_buffer_full) {
    // There are still audio data not appended so schedule a callback later.
    SbTimeMonotonic delay = 0;
    int64_t frames_in_buffer =
        frames_sent_to_sink_.load() - frames_consumed_by_sink_.load();
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

  if (seeking_.load() && time_stretcher_.IsQueueFull()) {
    seeking_.store(false);
  }

  if (seeking_.load() || playback_rate_.load() == 0.0) {
    return false;
  }

  int frames_in_buffer = static_cast<int>(frames_sent_to_sink_.load() -
                                          frames_consumed_by_sink_.load());

  if (max_cached_frames_ - frames_in_buffer < max_frames_per_append_) {
    *is_frame_buffer_full = true;
    return false;
  }

  int offset_to_append = frames_sent_to_sink_.load() % max_cached_frames_;

  scoped_refptr<DecodedAudio> decoded_audio =
      time_stretcher_.Read(max_frames_per_append_, playback_rate_.load());
  SB_DCHECK(decoded_audio);
  if (decoded_audio->frames() == 0 && eos_state_.load() == kEOSDecoded) {
    eos_state_.store(kEOSSentToSink);
  }
  audio_frame_tracker_.AddFrames(decoded_audio->frames(),
                                 playback_rate_.load());
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

  frames_sent_to_sink_.fetch_add(frames_appended);

  return frames_appended > 0;
}

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
