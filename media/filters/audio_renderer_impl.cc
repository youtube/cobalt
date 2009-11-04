// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>

#include "media/base/filter_host.h"
#include "media/filters/audio_renderer_impl.h"

namespace media {

// We'll try to fill 4096 samples per buffer, which is roughly ~92ms of audio
// data for a 44.1kHz audio source.
static const size_t kSamplesPerBuffer = 8*1024;

AudioRendererImpl::AudioRendererImpl()
    : AudioRendererBase(),
      stream_(NULL),
      bytes_per_second_(0) {
}

AudioRendererImpl::~AudioRendererImpl() {
  // Close down the audio device.
  if (stream_) {
    stream_->Stop();
    stream_->Close();
  }
}

bool AudioRendererImpl::IsMediaFormatSupported(
    const MediaFormat& media_format) {
  int channels;
  int sample_rate;
  int sample_bits;
  return AudioManager::GetAudioManager()->HasAudioDevices() &&
      ParseMediaFormat(media_format, &channels, &sample_rate, &sample_bits);
}

void AudioRendererImpl::SetPlaybackRate(float rate) {
  // TODO(fbarchard): limit rate to reasonable values
  AudioRendererBase::SetPlaybackRate(rate);

  static bool started = false;
  if (rate > 0.0f && !started && stream_)
    stream_->Start(this);
}

void AudioRendererImpl::SetVolume(float volume) {
  if (stream_)
    stream_->SetVolume(volume);
}

size_t AudioRendererImpl::OnMoreData(AudioOutputStream* stream, void* dest_void,
                                     size_t len, int pending_bytes) {
  // TODO(scherkus): handle end of stream.
  if (!stream_)
    return 0;

  // TODO(scherkus): Maybe change OnMoreData to pass in char/uint8 or similar.
  // TODO(fbarchard): Waveout_output_win.h should handle zero length buffers
  //                  without clicking.
  pending_bytes = static_cast<int>(ceil(pending_bytes * GetPlaybackRate()));
  base::TimeDelta delay =  base::TimeDelta::FromMicroseconds(
      base::Time::kMicrosecondsPerSecond * pending_bytes / bytes_per_second_);
  return FillBuffer(static_cast<uint8*>(dest_void), len, delay);
}

void AudioRendererImpl::OnClose(AudioOutputStream* stream) {
  // TODO(scherkus): implement OnClose.
  NOTIMPLEMENTED();
}

void AudioRendererImpl::OnError(AudioOutputStream* stream, int code) {
  // TODO(scherkus): implement OnError.
  NOTIMPLEMENTED();
}

bool AudioRendererImpl::OnInitialize(const MediaFormat& media_format) {
  // Parse out audio parameters.
  int channels;
  int sample_rate;
  int sample_bits;
  if (!ParseMediaFormat(media_format, &channels, &sample_rate, &sample_bits)) {
    return false;
  }

  bytes_per_second_ = sample_rate * channels * sample_bits / 8;

  // Create our audio stream.
  stream_ = AudioManager::GetAudioManager()->MakeAudioStream(
      AudioManager::AUDIO_PCM_LINEAR, channels, sample_rate, sample_bits);
  if (!stream_)
    return false;

  // Calculate buffer size and open the stream.
  size_t size = kSamplesPerBuffer * channels * sample_bits / 8;
  if (!stream_->Open(size)) {
    stream_->Close();
    stream_ = NULL;
  }
  return true;
}

void AudioRendererImpl::OnStop() {
  if (stream_)
    stream_->Stop();
}

}  // namespace media
