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

#include "starboard/shared/starboard/player/audio_renderer_internal.h"

#include <algorithm>

#include "starboard/memory.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {

namespace {
// TODO: This should be retrieved from the decoder.
// TODO: Make it not dependent on the frame size of AAC and HE-AAC.
const int kMaxFramesPerAccessUnit = 1024 * 2;
}  // namespace

AudioRenderer::AudioRenderer(AudioDecoder* decoder,
                             const SbMediaAudioHeader& audio_header)
    : channels_(audio_header.number_of_channels),
      paused_(true),
      seeking_(false),
      seeking_to_pts_(0),
      frame_buffer_(kMaxCachedFrames * audio_header.number_of_channels),
      frames_in_buffer_(0),
      offset_in_frames_(0),
      frames_consumed_(0),
      end_of_stream_reached_(false),
      decoder_(decoder),
      audio_sink_(kSbAudioSinkInvalid) {
  SB_DCHECK(decoder_ != NULL);
  frame_buffers_[0] = &frame_buffer_[0];
}

AudioRenderer::~AudioRenderer() {
  if (audio_sink_ != kSbAudioSinkInvalid) {
    SbAudioSinkDestroy(audio_sink_);
  }
  delete decoder_;
}

void AudioRenderer::WriteSample(const InputBuffer& input_buffer) {
  if (end_of_stream_reached_) {
    SB_LOG(ERROR) << "Appending audio sample at " << input_buffer.pts()
                  << " after EOS reached.";
    return;
  }

  SbMediaTime input_pts = input_buffer.pts();
  std::vector<float> decoded_audio;
  decoder_->Decode(input_buffer, &decoded_audio);
  SB_DCHECK(!decoded_audio.empty());

  ScopedLock lock(mutex_);
  if (seeking_) {
    if (input_pts < seeking_to_pts_) {
      return;
    }
  }

  AppendFrames(&decoded_audio[0], decoded_audio.size() / channels_);

  if (seeking_ && frame_buffer_.size() > kPrerollFrames * channels_) {
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
        channels_, sample_rate, kSbMediaAudioSampleTypeFloat32,
        kSbMediaAudioFrameStorageTypeInterleaved,
        reinterpret_cast<SbAudioSinkFrameBuffers>(frame_buffers_),
        kMaxCachedFrames, &AudioRenderer::UpdateSourceStatusFunc,
        &AudioRenderer::ConsumeFramesFunc, this);
  }
}

void AudioRenderer::WriteEndOfStream() {
  SB_LOG_IF(WARNING, end_of_stream_reached_)
      << "Try to write EOS after EOS is reached";
  if (end_of_stream_reached_) {
    return;
  }
  end_of_stream_reached_ = true;
  decoder_->WriteEndOfStream();

  ScopedLock lock(mutex_);
  // If we are seeking, we consider the seek is finished if end of stream is
  // reached as there won't be any audio data in future.
  if (seeking_) {
    seeking_ = false;
  }
}

void AudioRenderer::Play() {
  ScopedLock lock(mutex_);
  paused_ = false;
}

void AudioRenderer::Pause() {
  ScopedLock lock(mutex_);
  paused_ = true;
}

void AudioRenderer::Seek(SbMediaTime seek_to_pts) {
  SB_DCHECK(seek_to_pts >= 0);

  SbAudioSinkDestroy(audio_sink_);
  audio_sink_ = kSbAudioSinkInvalid;

  seeking_to_pts_ = std::max<SbMediaTime>(seek_to_pts, 0);
  seeking_ = true;
  frames_in_buffer_ = 0;
  offset_in_frames_ = 0;
  frames_consumed_ = 0;
  end_of_stream_reached_ = false;

  decoder_->Reset();
  return;
}

bool AudioRenderer::IsEndOfStreamPlayed() const {
  return end_of_stream_reached_ && frames_in_buffer_ == 0;
}

bool AudioRenderer::CanAcceptMoreData() const {
  ScopedLock lock(mutex_);
  return frames_in_buffer_ <= kMaxCachedFrames - kMaxFramesPerAccessUnit &&
         !end_of_stream_reached_;
}

bool AudioRenderer::IsSeekingInProgress() const {
  return seeking_;
}

SbMediaTime AudioRenderer::GetCurrentTime() {
  if (seeking_) {
    return seeking_to_pts_;
  }
  return seeking_to_pts_ +
         frames_consumed_ * kSbMediaTimeSecond /
             decoder_->GetSamplesPerSecond();
}

// static
void AudioRenderer::UpdateSourceStatusFunc(int* frames_in_buffer,
                                           int* offset_in_frames,
                                           bool* is_playing,
                                           bool* is_eos_reached,
                                           void* context) {
  AudioRenderer* audio_renderer = reinterpret_cast<AudioRenderer*>(context);
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
  AudioRenderer* audio_renderer = reinterpret_cast<AudioRenderer*>(context);
  SB_DCHECK(audio_renderer);

  audio_renderer->ConsumeFrames(frames_consumed);
}

void AudioRenderer::UpdateSourceStatus(int* frames_in_buffer,
                                       int* offset_in_frames,
                                       bool* is_playing,
                                       bool* is_eos_reached) {
  ScopedLock lock(mutex_);

  *is_eos_reached = end_of_stream_reached_;

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
}

void AudioRenderer::AppendFrames(const float* source_buffer,
                                 int frames_to_append) {
  SB_DCHECK(frames_in_buffer_ + frames_to_append <= kMaxCachedFrames);

  int offset_to_append =
      (offset_in_frames_ + frames_in_buffer_) % kMaxCachedFrames;
  if (frames_to_append > kMaxCachedFrames - offset_to_append) {
    SbMemoryCopy(
        &frame_buffer_[offset_to_append * channels_], source_buffer,
        (kMaxCachedFrames - offset_to_append) * sizeof(float) * channels_);
    source_buffer += (kMaxCachedFrames - offset_to_append) * channels_;
    frames_to_append -= kMaxCachedFrames - offset_to_append;
    frames_in_buffer_ += kMaxCachedFrames - offset_to_append;
    offset_to_append = 0;
  }
  SbMemoryCopy(&frame_buffer_[offset_to_append * channels_], source_buffer,
               frames_to_append * sizeof(float) * channels_);
  frames_in_buffer_ += frames_to_append;
}

}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
