// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/null_audio_sink.h"

#include "base/bind.h"
#include "base/stringprintf.h"
#include "base/sys_byteorder.h"
#include "base/threading/platform_thread.h"

namespace media {

NullAudioSink::NullAudioSink()
    : initialized_(false),
      playing_(false),
      callback_(NULL),
      thread_("NullAudioThread"),
      hash_audio_for_testing_(false) {
}

void NullAudioSink::Initialize(const AudioParameters& params,
                               RenderCallback* callback) {
  DCHECK(!initialized_);
  params_ = params;

  audio_bus_ = AudioBus::Create(params_);

  if (hash_audio_for_testing_) {
    md5_channel_contexts_.reset(new base::MD5Context[params_.channels()]);
    for (int i = 0; i < params_.channels(); i++)
      base::MD5Init(&md5_channel_contexts_[i]);
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

bool NullAudioSink::SetVolume(double volume) {
  // Audio is always muted.
  return volume == 0.0;
}

void NullAudioSink::SetPlaying(bool is_playing) {
  base::AutoLock auto_lock(lock_);
  playing_ = is_playing;
}

NullAudioSink::~NullAudioSink() {
  DCHECK(!thread_.IsRunning());
}

void NullAudioSink::FillBufferTask() {
  base::AutoLock auto_lock(lock_);

  base::TimeDelta delay;
  // Only consume buffers when actually playing.
  if (playing_)  {
    int frames_received = callback_->Render(audio_bus_.get(), 0);
    int frames_per_millisecond =
        params_.sample_rate() / base::Time::kMillisecondsPerSecond;

    if (hash_audio_for_testing_ && frames_received > 0) {
      DCHECK_EQ(sizeof(float), sizeof(uint32));
      int channels = audio_bus_->channels();
      for (int channel_idx = 0; channel_idx < channels; ++channel_idx) {
        float* channel = audio_bus_->channel(channel_idx);
        for (int frame_idx = 0; frame_idx < frames_received; frame_idx++) {
          // Convert float to uint32 w/o conversion loss.
          uint32 frame = base::ByteSwapToLE32(
              bit_cast<uint32>(channel[frame_idx]));
          base::MD5Update(
              &md5_channel_contexts_[channel_idx], base::StringPiece(
                  reinterpret_cast<char*>(&frame), sizeof(frame)));
        }
      }
    }

    // Calculate our sleep duration.
    delay = base::TimeDelta::FromMilliseconds(
        frames_received / frames_per_millisecond);
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
  DCHECK(!initialized_);
  hash_audio_for_testing_ = true;
}

std::string NullAudioSink::GetAudioHashForTesting() {
  DCHECK(hash_audio_for_testing_);

  // If initialize failed or was never called, ensure we return an empty hash.
  if (!initialized_) {
    md5_channel_contexts_.reset(new base::MD5Context[1]);
    base::MD5Init(&md5_channel_contexts_[0]);
  }

  // Hash all channels into the first channel.
  base::MD5Digest digest;
  for (int i = 1; i < audio_bus_->channels(); i++) {
    base::MD5Final(&digest, &md5_channel_contexts_[i]);
    base::MD5Update(&md5_channel_contexts_[0], base::StringPiece(
        reinterpret_cast<char*>(&digest), sizeof(base::MD5Digest)));
  }

  base::MD5Final(&digest, &md5_channel_contexts_[0]);
  return base::MD5DigestToBase16(digest);
}

}  // namespace media
