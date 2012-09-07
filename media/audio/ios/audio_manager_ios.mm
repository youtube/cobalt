// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/ios/audio_manager_ios.h"

#import <AudioToolbox/AudioToolbox.h>

#include "base/sys_info.h"
#include "media/audio/fake_audio_input_stream.h"
#include "media/audio/mac/audio_input_mac.h"
#include "media/base/limits.h"

namespace media {

enum { kMaxInputChannels = 2 };

// Initializes the audio session, returning a bool indicating whether
// initialization was successful. Should only be called once.
static bool InitAudioSessionInternal() {
  OSStatus error = AudioSessionInitialize(NULL, NULL, NULL, NULL);
  DCHECK(error != kAudioSessionAlreadyInitialized);
  return error == kAudioSessionNoError;
}

AudioManagerIOS::AudioManagerIOS() {
}

AudioManagerIOS::~AudioManagerIOS() {
  Shutdown();
}

bool AudioManagerIOS::HasAudioOutputDevices() {
  return false;
}

bool AudioManagerIOS::HasAudioInputDevices() {
  if (!InitAudioSession())
    return false;
  // Note that the |kAudioSessionProperty_AudioInputAvailable| property is a
  // 32-bit integer, not a boolean.
  UInt32 property_size;
  OSStatus error =
      AudioSessionGetPropertySize(kAudioSessionProperty_AudioInputAvailable,
                                  &property_size);
  if (error != kAudioSessionNoError)
    return false;
  UInt32 audio_input_is_available = false;
  DCHECK(property_size == sizeof(audio_input_is_available));
  error = AudioSessionGetProperty(kAudioSessionProperty_AudioInputAvailable,
                                  &property_size,
                                  &audio_input_is_available);
  return error == kAudioSessionNoError ? audio_input_is_available : false;
}

AudioOutputStream* AudioManagerIOS::MakeAudioOutputStream(
    const AudioParameters& params) {
  NOTIMPLEMENTED();  // Only input is supported on iOS.
  return NULL;
}

AudioInputStream* AudioManagerIOS::MakeAudioInputStream(
    const AudioParameters& params, const std::string& device_id) {
  // Current line of iOS devices has only one audio input.
  // Ignore the device_id (unittest uses a test value in it).
  if (!params.IsValid() || (params.channels() > kMaxInputChannels))
    return NULL;

  if (params.format() == AudioParameters::AUDIO_MOCK)
    return FakeAudioInputStream::MakeFakeStream(this, params);
  else if (params.format() == AudioParameters::AUDIO_PCM_LINEAR)
    return new PCMQueueInAudioInputStream(this, params);
  return NULL;
}

AudioOutputStream* AudioManagerIOS::MakeLinearOutputStream(
      const AudioParameters& params) {
  NOTIMPLEMENTED();  // Only input is supported on iOS.
  return NULL;
}

AudioOutputStream* AudioManagerIOS::MakeLowLatencyOutputStream(
      const AudioParameters& params) {
  NOTIMPLEMENTED();  // Only input is supported on iOS.
  return NULL;
}

AudioInputStream* AudioManagerIOS::MakeLinearInputStream(
      const AudioParameters& params, const std::string& device_id) {
  return MakeAudioInputStream(params, device_id);
}

AudioInputStream* AudioManagerIOS::MakeLowLatencyInputStream(
      const AudioParameters& params, const std::string& device_id) {
  NOTIMPLEMENTED();  // Only linear audio input is supported on iOS.
  return MakeAudioInputStream(params, device_id);
}

// Called by the stream when it has been released by calling Close().
void AudioManagerIOS::ReleaseOutputStream(AudioOutputStream* stream) {
  NOTIMPLEMENTED();  // Only input is supported on iOS.
}

// Called by the stream when it has been released by calling Close().
void AudioManagerIOS::ReleaseInputStream(AudioInputStream* stream) {
  delete stream;
}

bool AudioManagerIOS::InitAudioSession() {
  static const bool kSessionInitialized = InitAudioSessionInternal();
  return kSessionInitialized;
}

// static
AudioManager* CreateAudioManager() {
  return new AudioManagerIOS();
}

}  // namespace media
