// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_io.h"

#include <windows.h>
#include <mmsystem.h>

#include "base/basictypes.h"
#include "media/audio/fake_audio_input_stream.h"
#include "media/audio/fake_audio_output_stream.h"
#include "media/audio/win/audio_manager_win.h"
#include "media/audio/win/wavein_input_win.h"
#include "media/audio/win/waveout_output_win.h"
#include "media/base/limits.h"

namespace {

// Up to 8 channels can be passed to the driver.
// This should work, given the right drivers, but graceful error handling is
// needed.
const int kWinMaxChannels = 8;

const int kWinMaxInputChannels = 2;
const int kMaxSamplesPerPacket = media::Limits::kMaxSampleRate;
// We use 3 buffers for recording audio so that if a recording callback takes
// some time to return we won't lose audio. More buffers while recording are
// ok because they don't introduce any delay in recording, unlike in playback
// where you first need to fill in that number of buffers before starting to
// play.
const int kNumInputBuffers = 3;

}  // namespace.

bool AudioManagerWin::HasAudioOutputDevices() {
  return (::waveOutGetNumDevs() != 0);
}

bool AudioManagerWin::HasAudioInputDevices() {
  return (::waveInGetNumDevs() != 0);
}

// Factory for the implementations of AudioOutputStream. Two implementations
// should suffice most windows user's needs.
// - PCMWaveOutAudioOutputStream: Based on the waveOutWrite API (in progress)
// - PCMDXSoundAudioOutputStream: Based on DirectSound or XAudio (future work).
AudioOutputStream* AudioManagerWin::MakeAudioOutputStream(
    AudioParameters params) {
  if (!params.IsValid() || (params.channels > kWinMaxChannels))
    return NULL;

  if (params.format == AudioParameters::AUDIO_MOCK) {
    return FakeAudioOutputStream::MakeFakeStream();
  } else if (params.format == AudioParameters::AUDIO_PCM_LINEAR) {
    return new PCMWaveOutAudioOutputStream(this, params, 3, WAVE_MAPPER);
  } else if (params.format == AudioParameters::AUDIO_PCM_LOW_LATENCY) {
    // TODO(cpu): waveout cannot hit 20ms latency. Use other method.
    return new PCMWaveOutAudioOutputStream(this, params, 2, WAVE_MAPPER);
  }
  return NULL;
}

// Factory for the implementations of AudioInputStream.
AudioInputStream* AudioManagerWin::MakeAudioInputStream(
    AudioParameters params, int samples_per_packet) {
  if (!params.IsValid() || (params.channels > kWinMaxInputChannels) ||
      (samples_per_packet > kMaxSamplesPerPacket) || (samples_per_packet < 0))
    return NULL;

  if (params.format == AudioParameters::AUDIO_MOCK) {
    return FakeAudioInputStream::MakeFakeStream(params, samples_per_packet);
  } else if (params.format == AudioParameters::AUDIO_PCM_LINEAR) {
    return new PCMWaveInAudioInputStream(this, params, kNumInputBuffers,
                                         samples_per_packet, WAVE_MAPPER);
  }
  return NULL;
}

void AudioManagerWin::ReleaseOutputStream(PCMWaveOutAudioOutputStream* stream) {
  if (stream)
    delete stream;
}

void AudioManagerWin::ReleaseInputStream(PCMWaveInAudioInputStream* stream) {
  if (stream)
    delete stream;
}

void AudioManagerWin::MuteAll() {
}

void AudioManagerWin::UnMuteAll() {
}

AudioManagerWin::~AudioManagerWin() {
}

// static
AudioManager* AudioManager::CreateAudioManager() {
  return new AudioManagerWin();
}
