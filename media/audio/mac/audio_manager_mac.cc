// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <CoreAudio/AudioHardware.h>

#include "base/at_exit.h"
#include "media/audio/fake_audio_output_stream.h"
#include "media/audio/mac/audio_manager_mac.h"
#include "media/audio/mac/audio_output_mac.h"

bool AudioManagerMac::HasAudioOutputDevices() {
  AudioDeviceID output_device_id = kAudioObjectUnknown;
  AudioObjectPropertyAddress property_address = {
    kAudioHardwarePropertyDefaultOutputDevice,  // mSelector
    kAudioObjectPropertyScopeGlobal,            // mScope
    kAudioObjectPropertyElementMaster           // mElement
  };
  size_t output_device_id_size = sizeof(output_device_id);
  OSStatus err = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                            &property_address,
                                            0,     // inQualifierDataSize
                                            NULL,  // inQualifierData
                                            &output_device_id_size,
                                            &output_device_id);
  return err == kAudioHardwareNoError &&
         output_device_id != kAudioObjectUnknown;
}

AudioOutputStream* AudioManagerMac::MakeAudioOutputStream(
    Format format,
    int channels,
    int sample_rate,
    char bits_per_sample) {
  if (format == AUDIO_MOCK)
    return FakeAudioOutputStream::MakeFakeStream();
  else if (format != AUDIO_PCM_LINEAR)
    return NULL;
  return new PCMQueueOutAudioOutputStream(this, channels, sample_rate,
                                          bits_per_sample);
}

void AudioManagerMac::MuteAll() {
  // TODO(cpu): implement.
}

void AudioManagerMac::UnMuteAll() {
  // TODO(cpu): implement.
}

// Called by the stream when it has been released by calling Close().
void AudioManagerMac::ReleaseOutputStream(
    PCMQueueOutAudioOutputStream* stream) {
  delete stream;
}

namespace {

AudioManagerMac* g_audio_manager = NULL;

}  // namespace.

void DestroyAudioManagerMac(void* param) {
  delete g_audio_manager;
  g_audio_manager = NULL;
}

// By convention, the AudioManager is not thread safe.
AudioManager* AudioManager::GetAudioManager() {
  if (!g_audio_manager) {
    g_audio_manager = new AudioManagerMac();
    base::AtExitManager::RegisterCallback(&DestroyAudioManagerMac, NULL);
  }
  return g_audio_manager;
}
