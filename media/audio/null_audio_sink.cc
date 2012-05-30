// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/null_audio_sink.h"

#include "base/bind.h"
#include "base/stringprintf.h"
#include "base/threading/platform_thread.h"

namespace media {

NullAudioSink::NullAudioSink()
    : initialized_(false),
      playback_rate_(0.0),
      playing_(false),
      callback_(NULL),
      thread_("NullAudioThread"),
      hash_audio_for_testing_(false) {
}

void NullAudioSink::Initialize(const AudioParameters& params,
                               RenderCallback* callback) {
  DCHECK(!initialized_);
  params_ = params;

  audio_data_.reserve(params.channels());
  for (int i = 0; i < params.channels(); ++i) {
    float* channel_data = new float[params.frames_per_buffer()];
    audio_data_.push_back(channel_data);
  }

  callback_ = callback;
  initialized_ = true;
}

void NullAudioSink::Start() {
  if (!thread_.Start())
    return;

  thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
      &NullAudioSink::FillBufferTask, this));
}

void NullAudioSink::Stop() {
  SetPlaying(false);
  thread_.Stop();
}

void NullAudioSink::Play() {
  SetPlaying(true);
}

void NullAudioSink::Pause(bool /* flush */) {
  SetPlaying(false);
}

void NullAudioSink::SetPlaybackRate(float rate) {
  base::AutoLock auto_lock(lock_);
  playback_rate_ = rate;
}

bool NullAudioSink::SetVolume(double volume) {
  // Audio is always muted.
  return volume == 0.0;
}

void NullAudioSink::GetVolume(double* volume) {
  // Audio is always muted.
  *volume = 0.0;
}

void NullAudioSink::SetPlaying(bool is_playing) {
  base::AutoLock auto_lock(lock_);
  playing_ = is_playing;
}

NullAudioSink::~NullAudioSink() {
  DCHECK(!thread_.IsRunning());
  for (size_t i = 0; i < audio_data_.size(); ++i)
    delete [] audio_data_[i];
}

void NullAudioSink::FillBufferTask() {
  base::AutoLock auto_lock(lock_);

  base::TimeDelta delay;
  // Only consume buffers when actually playing.
  if (playing_)  {
    DCHECK_GT(playback_rate_, 0.0f);
    int requested_frames = params_.frames_per_buffer();
    int frames_received = callback_->Render(audio_data_, requested_frames, 0);
    int frames_per_millisecond =
        params_.sample_rate() / base::Time::kMillisecondsPerSecond;

    if (hash_audio_for_testing_) {
      int channels = audio_data_.size();
      // Include hash of channel count in case frames_received == 0.
      base::MD5Update(&md5_context_, base::StringPiece(
          base::StringPrintf("%d", channels)));
      for (int channel_index = 0; channel_index < channels; ++channel_index) {
        base::MD5Update(&md5_context_, base::StringPiece(
            reinterpret_cast<char*>(audio_data_[channel_index]),
            sizeof(float) * frames_received));
      }
    }

    // Calculate our sleep duration, taking playback rate into consideration.
    delay = base::TimeDelta::FromMilliseconds(
        frames_received / (frames_per_millisecond * playback_rate_));
  } else {
    // If paused, sleep for 10 milliseconds before polling again.
    delay = base::TimeDelta::FromMilliseconds(10);
  }

  // Sleep for at least one millisecond so we don't spin the CPU.
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&NullAudioSink::FillBufferTask, this),
      std::max(delay, base::TimeDelta::FromMilliseconds(1)));
}

void NullAudioSink::StartAudioHashForTesting() {
  hash_audio_for_testing_ = true;
  base::MD5Init(&md5_context_);
}

std::string NullAudioSink::GetAudioHashForTesting() {
  DCHECK(hash_audio_for_testing_);
  base::MD5Digest digest;
  base::MD5Final(&digest, &md5_context_);
  return base::MD5DigestToBase16(digest);
}

}  // namespace media
