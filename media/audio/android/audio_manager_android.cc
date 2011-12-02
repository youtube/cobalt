// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/android/audio_manager_android.h"

#include "base/logging.h"
#include "media/audio/android/audio_track_output_android.h"
#include "media/audio/audio_manager.h"
#include "media/audio/fake_audio_input_stream.h"
#include "media/audio/fake_audio_output_stream.h"

// static
AudioManager* AudioManager::CreateAudioManager() {
  return new AudioManagerAndroid();
}

AudioManagerAndroid::AudioManagerAndroid() {}

AudioManagerAndroid::~AudioManagerAndroid() {
  audio_thread_.Stop();
}

bool AudioManagerAndroid::HasAudioOutputDevices() {
  return true;
}

bool AudioManagerAndroid::HasAudioInputDevices() {
  return false;
}

AudioOutputStream* AudioManagerAndroid::MakeAudioOutputStream(
    const AudioParameters& params) {
  if (!params.IsValid())
    return NULL;

  if (params.format == AudioParameters::AUDIO_MOCK)
    return FakeAudioOutputStream::MakeFakeStream(params);
  if (params.format == AudioParameters::AUDIO_PCM_LINEAR ||
      params.format == AudioParameters::AUDIO_PCM_LOW_LATENCY)
    return AudioTrackOutputStream::MakeStream(params);

  return NULL;
}

AudioInputStream* AudioManagerAndroid::MakeAudioInputStream(
    const AudioParameters& params, const std::string& device_id) {
  return FakeAudioInputStream::MakeFakeStream(params);
}

void AudioManagerAndroid::MuteAll() {
  NOTIMPLEMENTED();
}

void AudioManagerAndroid::UnMuteAll() {
  NOTIMPLEMENTED();
}
