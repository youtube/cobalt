// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/audio_renderer_algorithm.h"

#include <algorithm>
#include <cmath>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "media/audio/audio_util.h"
#include "media/base/buffers.h"

namespace media {

// The starting size in bytes for |audio_buffer_|.
// Previous usage maintained a deque of 16 Buffers, each of size 4Kb. This
// worked well, so we maintain this number of bytes (16 * 4096).
static const int kStartingBufferSizeInBytes = 65536;

// The maximum size in bytes for the |audio_buffer_|. Arbitrarily determined.
// This number represents 3 seconds of 96kHz/16 bit 7.1 surround sound.
static const int kMaxBufferSizeInBytes = 4608000;

// Duration of audio segments used for crossfading (in seconds).
static const double kWindowDuration = 0.08;

// Duration of crossfade between audio segments (in seconds).
static const double kCrossfadeDuration = 0.008;

// Max/min supported playback rates for fast/slow audio. Audio outside of these
// ranges are muted.
// Audio at these speeds would sound better under a frequency domain algorithm.
static const float kMinPlaybackRate = 0.5f;
static const float kMaxPlaybackRate = 4.0f;

AudioRendererAlgorithm::AudioRendererAlgorithm()
    : channels_(0),
      samples_per_second_(0),
      bytes_per_channel_(0),
      playback_rate_(0.0f),
      audio_buffer_(0, kStartingBufferSizeInBytes),
      bytes_in_crossfade_(0),
      bytes_per_frame_(0),
      index_into_window_(0),
      crossfade_frame_number_(0),
      muted_(false),
      needs_more_data_(false),
      window_size_(0) {
}

AudioRendererAlgorithm::~AudioRendererAlgorithm() {}

bool AudioRendererAlgorithm::ValidateConfig(
    int channels,
    int samples_per_second,
    int bits_per_channel) {
  bool status = true;

  if (channels <= 0 || channels > 8) {
    DVLOG(1) << "We only support audio with between 1 and 8 channels.";
    status = false;
  }

  if (samples_per_second <= 0 || samples_per_second > 256000) {
    DVLOG(1) << "We only support sample rates between 1 and 256000Hz.";
    status = false;
  }

  if (bits_per_channel != 8 && bits_per_channel != 16 &&
      bits_per_channel != 32) {
    DVLOG(1) << "We only support 8, 16, 32 bit audio.";
    status = false;
  }

  return status;
}

void AudioRendererAlgorithm::Initialize(
    int channels,
    int samples_per_second,
    int bits_per_channel,
    float initial_playback_rate,
    const base::Closure& callback) {
  DCHECK(!callback.is_null());
  DCHECK(ValidateConfig(channels, samples_per_second, bits_per_channel));

  channels_ = channels;
  samples_per_second_ = samples_per_second;
  bytes_per_channel_ = bits_per_channel / 8;
  bytes_per_frame_ = bytes_per_channel_ * channels_;
  request_read_cb_ = callback;
  SetPlaybackRate(initial_playback_rate);

  window_size_ =
      samples_per_second_ * bytes_per_channel_ * channels_ * kWindowDuration;
  AlignToFrameBoundary(&window_size_);

  bytes_in_crossfade_ =
      samples_per_second_ * bytes_per_channel_ * channels_ * kCrossfadeDuration;
  AlignToFrameBoundary(&bytes_in_crossfade_);

  crossfade_buffer_.reset(new uint8[bytes_in_crossfade_]);
}

int AudioRendererAlgorithm::FillBuffer(
    uint8* dest, int requested_frames) {
  DCHECK_NE(bytes_per_frame_, 0);

  if (playback_rate_ == 0.0f)
    return 0;

  int total_frames_rendered = 0;
  uint8* output_ptr = dest;
  while (total_frames_rendered < requested_frames) {
    if (index_into_window_ == window_size_)
      ResetWindow();

    bool rendered_frame = true;
    if (playback_rate_ > 1.0)
      rendered_frame = OutputFasterPlayback(output_ptr);
    else if (playback_rate_ < 1.0)
      rendered_frame = OutputSlowerPlayback(output_ptr);
    else
      rendered_frame = OutputNormalPlayback(output_ptr);

    if (!rendered_frame) {
      needs_more_data_ = true;
      break;
    }

    output_ptr += bytes_per_frame_;
    total_frames_rendered++;
  }
  return total_frames_rendered;
}

void AudioRendererAlgorithm::ResetWindow() {
  DCHECK_LE(index_into_window_, window_size_);
  index_into_window_ = 0;
  crossfade_frame_number_ = 0;
}

bool AudioRendererAlgorithm::OutputFasterPlayback(uint8* dest) {
  DCHECK_LT(index_into_window_, window_size_);
  DCHECK_GT(playback_rate_, 1.0);

  if (audio_buffer_.forward_bytes() < bytes_per_frame_)
    return false;

  // The audio data is output in a series of windows. For sped-up playback,
  // the window is comprised of the following phases:
  //
  //  a) Output raw data.
  //  b) Save bytes for crossfade in |crossfade_buffer_|.
  //  c) Drop data.
  //  d) Output crossfaded audio leading up to the next window.
  //
  // The duration of each phase is computed below based on the |window_size_|
  // and |playback_rate_|.
  int input_step = window_size_;
  int output_step = ceil(window_size_ / playback_rate_);
  AlignToFrameBoundary(&output_step);
  DCHECK_GT(input_step, output_step);

  int bytes_to_crossfade = bytes_in_crossfade_;
  if (muted_ || bytes_to_crossfade > output_step)
    bytes_to_crossfade = 0;

  // This is the index of the end of phase a, beginning of phase b.
  int outtro_crossfade_begin = output_step - bytes_to_crossfade;

  // This is the index of the end of phase b, beginning of phase c.
  int outtro_crossfade_end = output_step;

  // This is the index of the end of phase c, beginning of phase d.
  // This phase continues until |index_into_window_| reaches |window_size_|, at
  // which point the window restarts.
  int intro_crossfade_begin = input_step - bytes_to_crossfade;

  // a) Output a raw frame if we haven't reached the crossfade section.
  if (index_into_window_ < outtro_crossfade_begin) {
    CopyWithAdvance(dest);
    index_into_window_ += bytes_per_frame_;
    return true;
  }

  // b) Save outtro crossfade frames into intermediate buffer, but do not output
  //    anything to |dest|.
  while (index_into_window_ < outtro_crossfade_end) {
    if (audio_buffer_.forward_bytes() < bytes_per_frame_)
      return false;

    // This phase only applies if there are bytes to crossfade.
    DCHECK_GT(bytes_to_crossfade, 0);
    uint8* place_to_copy = crossfade_buffer_.get() +
        (index_into_window_ - outtro_crossfade_begin);
    CopyWithAdvance(place_to_copy);
    index_into_window_ += bytes_per_frame_;
  }

  // c) Drop frames until we reach the intro crossfade section.
  while (index_into_window_ < intro_crossfade_begin) {
    if (audio_buffer_.forward_bytes() < bytes_per_frame_)
      return false;

    DropFrame();
    index_into_window_ += bytes_per_frame_;
  }

  // Return if we have run out of data after Phase c).
  if (audio_buffer_.forward_bytes() < bytes_per_frame_)
    return false;

  // Phase d) doesn't apply if there are no bytes to crossfade.
  if (bytes_to_crossfade == 0) {
    DCHECK_EQ(index_into_window_, window_size_);
    return false;
  }

  // d) Crossfade and output a frame.
  DCHECK_LT(index_into_window_, window_size_);
  int offset_into_buffer = index_into_window_ - intro_crossfade_begin;
  memcpy(dest, crossfade_buffer_.get() + offset_into_buffer,
         bytes_per_frame_);
  scoped_array<uint8> intro_frame_ptr(new uint8[bytes_per_frame_]);
  audio_buffer_.Read(intro_frame_ptr.get(), bytes_per_frame_);
  OutputCrossfadedFrame(dest, intro_frame_ptr.get());
  index_into_window_ += bytes_per_frame_;
  return true;
}

bool AudioRendererAlgorithm::OutputSlowerPlayback(uint8* dest) {
  DCHECK_LT(index_into_window_, window_size_);
  DCHECK_LT(playback_rate_, 1.0);
  DCHECK_NE(playback_rate_, 0.0);

  if (audio_buffer_.forward_bytes() < bytes_per_frame_)
    return false;

  // The audio data is output in a series of windows. For slowed down playback,
  // the window is comprised of the following phases:
  //
  //  a) Output raw data.
  //  b) Output and save bytes for crossfade in |crossfade_buffer_|.
  //  c) Output* raw data.
  //  d) Output* crossfaded audio leading up to the next window.
  //
  // * Phases c) and d) do not progress |audio_buffer_|'s cursor so that the
  // |audio_buffer_|'s cursor is in the correct place for the next window.
  //
  // The duration of each phase is computed below based on the |window_size_|
  // and |playback_rate_|.
  int input_step = ceil(window_size_ * playback_rate_);
  AlignToFrameBoundary(&input_step);
  int output_step = window_size_;
  DCHECK_LT(input_step, output_step);

  int bytes_to_crossfade = bytes_in_crossfade_;
  if (muted_ || bytes_to_crossfade > input_step)
    bytes_to_crossfade = 0;

  // This is the index of the end of phase a, beginning of phase b.
  int intro_crossfade_begin = input_step - bytes_to_crossfade;

  // This is the index of the end of phase b, beginning of phase c.
  int intro_crossfade_end = input_step;

  // This is the index of the end of phase c,  beginning of phase d.
  // This phase continues until |index_into_window_| reaches |window_size_|, at
  // which point the window restarts.
  int outtro_crossfade_begin = output_step - bytes_to_crossfade;

  // a) Output a raw frame.
  if (index_into_window_ < intro_crossfade_begin) {
    CopyWithAdvance(dest);
    index_into_window_ += bytes_per_frame_;
    return true;
  }

  // b) Save the raw frame for the intro crossfade section, then output the
  //    frame to |dest|.
  if (index_into_window_ < intro_crossfade_end) {
    int offset = index_into_window_ - intro_crossfade_begin;
    uint8* place_to_copy = crossfade_buffer_.get() + offset;
    CopyWithoutAdvance(place_to_copy);
    CopyWithAdvance(dest);
    index_into_window_ += bytes_per_frame_;
    return true;
  }

  int audio_buffer_offset = index_into_window_ - intro_crossfade_end;

  if (audio_buffer_.forward_bytes() < audio_buffer_offset + bytes_per_frame_)
    return false;

  // c) Output a raw frame into |dest| without advancing the |audio_buffer_|
  //    cursor. See function-level comment.
  DCHECK_GE(index_into_window_, intro_crossfade_end);
  CopyWithoutAdvance(dest, audio_buffer_offset);

  // d) Crossfade the next frame of |crossfade_buffer_| into |dest| if we've
  //    reached the outtro crossfade section of the window.
  if (index_into_window_ >= outtro_crossfade_begin) {
    int offset_into_crossfade_buffer =
        index_into_window_ - outtro_crossfade_begin;
    uint8* intro_frame_ptr =
        crossfade_buffer_.get() + offset_into_crossfade_buffer;
    OutputCrossfadedFrame(dest, intro_frame_ptr);
  }

  index_into_window_ += bytes_per_frame_;
  return true;
}

bool AudioRendererAlgorithm::OutputNormalPlayback(uint8* dest) {
  if (audio_buffer_.forward_bytes() >= bytes_per_frame_) {
    CopyWithAdvance(dest);
    index_into_window_ += bytes_per_frame_;
    return true;
  }
  return false;
}

void AudioRendererAlgorithm::CopyWithAdvance(uint8* dest) {
  CopyWithoutAdvance(dest);
  DropFrame();
}

void AudioRendererAlgorithm::CopyWithoutAdvance(uint8* dest) {
  CopyWithoutAdvance(dest, 0);
}

void AudioRendererAlgorithm::CopyWithoutAdvance(
    uint8* dest, int offset) {
  if (muted_) {
    memset(dest, 0, bytes_per_frame_);
    return;
  }
  int copied = audio_buffer_.Peek(dest, bytes_per_frame_, offset);
  DCHECK_EQ(bytes_per_frame_, copied);
}

void AudioRendererAlgorithm::DropFrame() {
  audio_buffer_.Seek(bytes_per_frame_);

  if (!IsQueueFull())
    request_read_cb_.Run();
}

void AudioRendererAlgorithm::OutputCrossfadedFrame(
    uint8* outtro, const uint8* intro) {
  DCHECK_LE(index_into_window_, window_size_);
  DCHECK(!muted_);

  switch (bytes_per_channel_) {
    case 4:
      CrossfadeFrame<int32>(outtro, intro);
      break;
    case 2:
      CrossfadeFrame<int16>(outtro, intro);
      break;
    case 1:
      CrossfadeFrame<uint8>(outtro, intro);
      break;
    default:
      NOTREACHED() << "Unsupported audio bit depth in crossfade.";
  }
}

template <class Type>
void AudioRendererAlgorithm::CrossfadeFrame(
    uint8* outtro_bytes, const uint8* intro_bytes) {
  Type* outtro = reinterpret_cast<Type*>(outtro_bytes);
  const Type* intro = reinterpret_cast<const Type*>(intro_bytes);

  int frames_in_crossfade = bytes_in_crossfade_ / bytes_per_frame_;
  float crossfade_ratio =
      static_cast<float>(crossfade_frame_number_) / frames_in_crossfade;
  for (int channel = 0; channel < channels_; ++channel) {
    *outtro *= 1.0 - crossfade_ratio;
    *outtro++ += (*intro++) * crossfade_ratio;
  }
  crossfade_frame_number_++;
}

void AudioRendererAlgorithm::SetPlaybackRate(float new_rate) {
  DCHECK_GE(new_rate, 0.0);
  playback_rate_ = new_rate;
  muted_ =
      playback_rate_ < kMinPlaybackRate || playback_rate_ > kMaxPlaybackRate;

  ResetWindow();
}

void AudioRendererAlgorithm::AlignToFrameBoundary(int* value) {
  (*value) -= ((*value) % bytes_per_frame_);
}

void AudioRendererAlgorithm::FlushBuffers() {
  ResetWindow();

  // Clear the queue of decoded packets (releasing the buffers).
  audio_buffer_.Clear();
  request_read_cb_.Run();
}

base::TimeDelta AudioRendererAlgorithm::GetTime() {
  return audio_buffer_.current_time();
}

void AudioRendererAlgorithm::EnqueueBuffer(Buffer* buffer_in) {
  DCHECK(!buffer_in->IsEndOfStream());
  audio_buffer_.Append(buffer_in);
  needs_more_data_ = false;

  // If we still don't have enough data, request more.
  if (!IsQueueFull())
    request_read_cb_.Run();
}

bool AudioRendererAlgorithm::CanFillBuffer() {
  return audio_buffer_.forward_bytes() > 0 && !needs_more_data_;
}

bool AudioRendererAlgorithm::IsQueueFull() {
  return audio_buffer_.forward_bytes() >= audio_buffer_.forward_capacity();
}

int AudioRendererAlgorithm::QueueCapacity() {
  return audio_buffer_.forward_capacity();
}

void AudioRendererAlgorithm::IncreaseQueueCapacity() {
  audio_buffer_.set_forward_capacity(
      std::min(2 * audio_buffer_.forward_capacity(), kMaxBufferSizeInBytes));
}

}  // namespace media
