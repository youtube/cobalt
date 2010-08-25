// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
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

namespace {

// The next 3 constants are some sensible limits to prevent integer overflow
// at this layer.
// Up to 6 channels can be passed to the driver.
// This should work, given the right drivers, but graceful error handling is
// needed.
// In theory 7.1 could also be supported, but it has not been tested.
// The 192 Khz constant is the frequency of quicktime lossless audio codec.
// MP4 is limited to 96 Khz, and mp3 is limited to 48 Khz.
// OGG vorbis was initially limited to 96 Khz, but recent tools are unlimited.
// 192 Khz is also the limit on most PC audio hardware.  The minimum is 100 Hz.
// Humans range is 20 to 20000 Hz.  Below 20 can be felt (woofer).

const int kMaxChannels = 6;
const int kMaxSampleRate = 192000;
const int kMaxBitsPerSample = 64;

const int kMaxInputChannels = 2;
const int kMaxSamplesPerPacket = kMaxSampleRate;
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
    Format format,
    int channels,
    int sample_rate,
    char bits_per_sample) {
  if ((channels > kMaxChannels) || (channels <= 0) ||
      (sample_rate > kMaxSampleRate) || (sample_rate <= 0) ||
      (bits_per_sample > kMaxBitsPerSample) || (bits_per_sample <= 0))
    return NULL;

  if (format == AUDIO_MOCK) {
    return FakeAudioOutputStream::MakeFakeStream();
  } else if (format == AUDIO_PCM_LINEAR) {
    return new PCMWaveOutAudioOutputStream(this, channels, sample_rate, 3,
                                           bits_per_sample, WAVE_MAPPER);
  } else if (format == AUDIO_PCM_LOW_LATENCY) {
    // TODO(cpu): waveout cannot hit 20ms latency. Use other method.
    return new PCMWaveOutAudioOutputStream(this, channels, sample_rate, 2,
                                           bits_per_sample, WAVE_MAPPER);
  }
  return NULL;
}

// Factory for the implementations of AudioInputStream.
AudioInputStream* AudioManagerWin::MakeAudioInputStream(
    Format format,
    int channels,
    int sample_rate,
    char bits_per_sample,
    uint32 samples_per_packet) {
  if ((channels > kMaxInputChannels) || (channels <= 0) ||
      (sample_rate > kMaxSampleRate) || (sample_rate <= 0) ||
      (bits_per_sample > kMaxBitsPerSample) || (bits_per_sample <= 0) ||
      (samples_per_packet > kMaxSamplesPerPacket) || (samples_per_packet < 0))
    return NULL;

  if (format == AUDIO_MOCK) {
    return FakeAudioInputStream::MakeFakeStream(channels, bits_per_sample,
                                                sample_rate,
                                                samples_per_packet);
  } else if (format == AUDIO_PCM_LINEAR) {
    return new PCMWaveInAudioInputStream(this, channels, sample_rate,
                                         kNumInputBuffers, bits_per_sample,
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
