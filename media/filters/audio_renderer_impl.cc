// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/audio_renderer_impl.h"

#include <math.h>

#include "base/logging.h"
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

bool AudioRendererImpl::OnInitialize(int bits_per_channel,
                                     ChannelLayout channel_layout,
                                     int sample_rate) {
  AudioParameters params(AudioParameters::AUDIO_PCM_LINEAR, channel_layout,
                         sample_rate, bits_per_channel, kSamplesPerBuffer);

  bytes_per_second_ = params.GetBytesPerSecond();

  // Create our audio stream.
  stream_ = AudioManager::GetAudioManager()->MakeAudioOutputStream(params);
  if (!stream_)
    return false;

  if (!stream_->Open()) {
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
