// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/audio_renderer_impl.h"

#include <math.h>

#include "media/base/filter_host.h"
#include "media/audio/audio_manager.h"

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
  return AudioManager::GetAudioManager()->HasAudioOutputDevices() &&
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

uint32 AudioRendererImpl::OnMoreData(
    AudioOutputStream* stream, uint8* dest, uint32 len,
    AudioBuffersState buffers_state) {
  // TODO(scherkus): handle end of stream.
  if (!stream_)
    return 0;

  // TODO(fbarchard): Waveout_output_win.h should handle zero length buffers
  //                  without clicking.
  uint32 pending_bytes = static_cast<uint32>(ceil(buffers_state.total_bytes() *
                                                  GetPlaybackRate()));
  base::TimeDelta delay = base::TimeDelta::FromMicroseconds(
      base::Time::kMicrosecondsPerSecond * pending_bytes /
      bytes_per_second_);
  bool buffers_empty = buffers_state.pending_bytes == 0;
  return FillBuffer(dest, len, delay, buffers_empty);
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
  AudioParameters params;
  if (!ParseMediaFormat(media_format, &params.channels,
                        &params.sample_rate, &params.bits_per_sample)) {
    return false;
  }

  bytes_per_second_ = params.sample_rate * params.channels *
      params.bits_per_sample / 8;

  // Create our audio stream.
  stream_ = AudioManager::GetAudioManager()->MakeAudioOutputStream(params);
  if (!stream_)
    return false;

  // Calculate buffer size and open the stream.
  size_t size = kSamplesPerBuffer * params.channels *
      params.bits_per_sample / 8;
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
