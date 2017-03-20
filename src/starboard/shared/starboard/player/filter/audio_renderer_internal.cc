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
#include "starboard/shared/starboard/audio_sink/audio_sink_internal.h"
#include "starboard/shared/starboard/player/closure.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

AudioRenderer::AudioRenderer(JobQueue* job_queue,
                             scoped_ptr<AudioDecoder> decoder,
                             const SbMediaAudioHeader& audio_header)
    : job_queue_(job_queue),
      channels_(audio_header.number_of_channels),
      bytes_per_frame_(
          (decoder->GetSampleType() == kSbMediaAudioSampleTypeInt16 ? 2 : 4) *
          channels_),
      playback_rate_(1.0),
      paused_(true),
      seeking_(false),
      seeking_to_pts_(0),
      frame_buffer_(kMaxCachedFrames * bytes_per_frame_),
      frames_in_buffer_(0),
      offset_in_frames_(0),
      frames_consumed_(0),
      end_of_stream_written_(false),
      end_of_stream_decoded_(false),
      decoder_(decoder.Pass()),
      audio_sink_(kSbAudioSinkInvalid) {
  SB_DCHECK(job_queue != NULL);
  SB_DCHECK(decoder_ != NULL);
  SB_DCHECK(job_queue_->BelongsToCurrentThread());

  frame_buffers_[0] = &frame_buffer_[0];
}

AudioRenderer::~AudioRenderer() {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());

  if (audio_sink_ != kSbAudioSinkInvalid) {
    SbAudioSinkDestroy(audio_sink_);
  }

  if (read_from_decoder_closure_.is_valid()) {
    job_queue_->Remove(read_from_decoder_closure_);
  }
}

void AudioRenderer::WriteSample(const InputBuffer& input_buffer) {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());

  if (end_of_stream_written_) {
    SB_LOG(ERROR) << "Appending audio sample at " << input_buffer.pts()
                  << " after EOS reached.";
    return;
  }

  decoder_->Decode(input_buffer);

  ScopedLock lock(mutex_);
  if (!read_from_decoder_closure_.is_valid()) {
    read_from_decoder_closure_ = Bind(&AudioRenderer::ReadFromDecoder, this);
    job_queue_->Schedule(read_from_decoder_closure_);
  }
}

void AudioRenderer::WriteEndOfStream() {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());

  SB_LOG_IF(WARNING, end_of_stream_written_)
      << "Try to write EOS after EOS is reached";
  if (end_of_stream_written_) {
    return;
  }
  decoder_->WriteEndOfStream();

  ScopedLock lock(mutex_);
  end_of_stream_written_ = true;
  // If we are seeking, we consider the seek is finished if end of stream is
  // reached as there won't be any audio data in future.
  if (seeking_) {
    seeking_ = false;
  }
}

void AudioRenderer::Play() {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());

  ScopedLock lock(mutex_);
  paused_ = false;
}

void AudioRenderer::Pause() {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());

  ScopedLock lock(mutex_);
  paused_ = true;
}

#if SB_API_VERSION >= SB_PLAYER_SET_PLAYBACK_RATE_VERSION
void AudioRenderer::SetPlaybackRate(double playback_rate) {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());

  playback_rate_ = playback_rate;

  if (audio_sink_) {
    audio_sink_->SetPlaybackRate(playback_rate);
  }
}
#endif  // SB_API_VERSION >= SB_PLAYER_SET_PLAYBACK_RATE_VERSION

void AudioRenderer::Seek(SbMediaTime seek_to_pts) {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());
  SB_DCHECK(seek_to_pts >= 0);

  SbAudioSinkDestroy(audio_sink_);
  // Now the sink is destroyed and the callbacks will no longer be called, so
  // the following modifications are safe without lock.
  audio_sink_ = kSbAudioSinkInvalid;

  seeking_to_pts_ = std::max<SbMediaTime>(seek_to_pts, 0);
  seeking_ = true;
  frames_in_buffer_ = 0;
  offset_in_frames_ = 0;
  frames_consumed_ = 0;
  end_of_stream_written_ = false;
  end_of_stream_decoded_ = false;
  pending_decoded_audio_ = NULL;

  decoder_->Reset();
}

bool AudioRenderer::IsEndOfStreamPlayed() const {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());

  ScopedLock lock(mutex_);
  return end_of_stream_decoded_ && frames_in_buffer_ == 0;
}

bool AudioRenderer::CanAcceptMoreData() const {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());

  ScopedLock lock(mutex_);
  if (end_of_stream_written_) {
    return false;
  }
  return pending_decoded_audio_ == NULL;
}

bool AudioRenderer::IsSeekingInProgress() const {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());
  return seeking_;
}

SbMediaTime AudioRenderer::GetCurrentTime() {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());

  if (seeking_) {
    return seeking_to_pts_;
  }
  return seeking_to_pts_ +
         frames_consumed_ * kSbMediaTimeSecond /
             decoder_->GetSamplesPerSecond();
}

void AudioRenderer::UpdateSourceStatus(int* frames_in_buffer,
                                       int* offset_in_frames,
                                       bool* is_playing,
                                       bool* is_eos_reached) {
  ScopedLock lock(mutex_);

  *is_eos_reached = end_of_stream_decoded_;

  if (paused_ || seeking_) {
    *is_playing = false;
    *frames_in_buffer = *offset_in_frames = 0;
    return;
  }

  *is_playing = true;
  *frames_in_buffer = frames_in_buffer_;
  *offset_in_frames = offset_in_frames_;
}

void AudioRenderer::ConsumeFrames(int frames_consumed) {
  ScopedLock lock(mutex_);

  SB_DCHECK(frames_consumed <= frames_in_buffer_);
  offset_in_frames_ += frames_consumed;
  offset_in_frames_ %= kMaxCachedFrames;
  frames_in_buffer_ -= frames_consumed;
  frames_consumed_ += frames_consumed;

  bool decoded_audio_available =
      pending_decoded_audio_ ||
      (end_of_stream_written_ && !end_of_stream_decoded_);
  if (decoded_audio_available && !read_from_decoder_closure_.is_valid()) {
    read_from_decoder_closure_ = Bind(&AudioRenderer::ReadFromDecoder, this);
    job_queue_->Schedule(read_from_decoder_closure_);
  }
}

// Try to read some audio data from the decoder.  Note that this operation is
// valid across seeking.  If a seek happens immediately after a ReadFromDecoder
// request is scheduled, the seek will reset the decoder.  So the
// ReadFromDecoder request will not read stale data.
void AudioRenderer::ReadFromDecoder() {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());

  ScopedLock lock(mutex_);
  SB_DCHECK(read_from_decoder_closure_.is_valid());
  read_from_decoder_closure_.reset();

  scoped_refptr<DecodedAudio> decoded_audio =
      pending_decoded_audio_ ? pending_decoded_audio_ : decoder_->Read();
  pending_decoded_audio_ = NULL;
  if (!decoded_audio) {
    return;
  }

  if (decoded_audio->is_end_of_stream()) {
    SB_DCHECK(end_of_stream_written_);
    end_of_stream_decoded_ = true;
    return;
  }

  if (seeking_) {
    if (decoded_audio->pts() < seeking_to_pts_) {
      // Discard any audio data before the seeking target.
      return;
    }
  }

  if (!AppendDecodedAudio_Locked(decoded_audio)) {
    pending_decoded_audio_ = decoded_audio;
    return;
  }

  if (seeking_ && frame_buffer_.size() > kPrerollFrames * bytes_per_frame_) {
    seeking_ = false;
  }

  // Create the audio sink if it is the first incoming AU after seeking.
  if (audio_sink_ == kSbAudioSinkInvalid) {
    int sample_rate = decoder_->GetSamplesPerSecond();
    // TODO: Implement resampler.
    SB_DCHECK(sample_rate ==
              SbAudioSinkGetNearestSupportedSampleFrequency(sample_rate));
    // TODO: Handle sink creation failure.
    audio_sink_ = SbAudioSinkCreate(
        channels_, sample_rate, decoder_->GetSampleType(),
        kSbMediaAudioFrameStorageTypeInterleaved,
        reinterpret_cast<SbAudioSinkFrameBuffers>(frame_buffers_),
        kMaxCachedFrames, &AudioRenderer::UpdateSourceStatusFunc,
        &AudioRenderer::ConsumeFramesFunc, this);
#if SB_API_VERSION >= SB_PLAYER_SET_PLAYBACK_RATE_VERSION
    audio_sink_->SetPlaybackRate(playback_rate_);
#endif  // SB_API_VERSION >= SB_PLAYER_SET_PLAYBACK_RATE_VERSION
  }
}

// TODO: This function should be executed when lock is not acquired as it copies
// relatively large amount of data.
bool AudioRenderer::AppendDecodedAudio_Locked(
    const scoped_refptr<DecodedAudio>& decoded_audio) {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());

  const uint8_t* source_buffer = decoded_audio->buffer();
  int frames_to_append = decoded_audio->size() / bytes_per_frame_;

  if (frames_in_buffer_ + frames_to_append > kMaxCachedFrames) {
    return false;
  }

  int offset_to_append =
      (offset_in_frames_ + frames_in_buffer_) % kMaxCachedFrames;
  if (frames_to_append > kMaxCachedFrames - offset_to_append) {
    SbMemoryCopy(&frame_buffer_[offset_to_append * bytes_per_frame_],
                 source_buffer,
                 (kMaxCachedFrames - offset_to_append) * bytes_per_frame_);
    source_buffer += (kMaxCachedFrames - offset_to_append) * bytes_per_frame_;
    frames_to_append -= kMaxCachedFrames - offset_to_append;
    frames_in_buffer_ += kMaxCachedFrames - offset_to_append;
    offset_to_append = 0;
  }
  SbMemoryCopy(&frame_buffer_[offset_to_append * bytes_per_frame_],
               source_buffer, frames_to_append * bytes_per_frame_);
  frames_in_buffer_ += frames_to_append;

  return true;
}

// static
void AudioRenderer::UpdateSourceStatusFunc(int* frames_in_buffer,
                                           int* offset_in_frames,
                                           bool* is_playing,
                                           bool* is_eos_reached,
                                           void* context) {
  AudioRenderer* audio_renderer = static_cast<AudioRenderer*>(context);
  SB_DCHECK(audio_renderer);
  SB_DCHECK(frames_in_buffer);
  SB_DCHECK(offset_in_frames);
  SB_DCHECK(is_playing);
  SB_DCHECK(is_eos_reached);

  audio_renderer->UpdateSourceStatus(frames_in_buffer, offset_in_frames,
                                     is_playing, is_eos_reached);
}

// static
void AudioRenderer::ConsumeFramesFunc(int frames_consumed, void* context) {
  AudioRenderer* audio_renderer = static_cast<AudioRenderer*>(context);
  SB_DCHECK(audio_renderer);

  audio_renderer->ConsumeFrames(frames_consumed);
}

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
