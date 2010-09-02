// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <CoreAudio/AudioHardware.h>

#include "media/audio/fake_audio_input_stream.h"
#include "media/audio/fake_audio_output_stream.h"
#include "media/audio/mac/audio_input_mac.h"
#include "media/audio/mac/audio_manager_mac.h"
#include "media/audio/mac/audio_output_mac.h"
#include "media/base/limits.h"

namespace {
const int kMaxInputChannels = 2;
const int kMaxSamplesPerPacket = media::Limits::kMaxSampleRate;

bool HasAudioHardware(AudioObjectPropertySelector selector) {
  AudioDeviceID output_device_id = kAudioObjectUnknown;
  const AudioObjectPropertyAddress property_address = {
    selector,
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
}  // namespace

bool AudioManagerMac::HasAudioOutputDevices() {
  return HasAudioHardware(kAudioHardwarePropertyDefaultOutputDevice);
}

bool AudioManagerMac::HasAudioInputDevices() {
  return HasAudioHardware(kAudioHardwarePropertyDefaultInputDevice);
}

AudioOutputStream* AudioManagerMac::MakeAudioOutputStream(
    AudioParameters params) {
  if (params.format == AudioParameters::AUDIO_MOCK)
    return FakeAudioOutputStream::MakeFakeStream();
  else if (params.format != AudioParameters::AUDIO_PCM_LINEAR)
    return NULL;
  return new PCMQueueOutAudioOutputStream(this, params);
}

AudioInputStream* AudioManagerMac::MakeAudioInputStream(
    AudioParameters params, int samples_per_packet) {
  if (!params.IsValid() || (params.channels > kMaxInputChannels) ||
      (samples_per_packet > kMaxSamplesPerPacket) || (samples_per_packet < 0))
    return NULL;

  if (params.format == AudioParameters::AUDIO_MOCK) {
    return FakeAudioInputStream::MakeFakeStream(params, samples_per_packet);
  } else if (params.format == AudioParameters::AUDIO_PCM_LINEAR) {
    return new PCMQueueInAudioInputStream(this, params, samples_per_packet);
  }
  return NULL;
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

// Called by the stream when it has been released by calling Close().
void AudioManagerMac::ReleaseInputStream(PCMQueueInAudioInputStream* stream) {
  delete stream;
}

// static
AudioManager* AudioManager::CreateAudioManager() {
  return new AudioManagerMac();
}
