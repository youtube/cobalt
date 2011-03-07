// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <CoreAudio/AudioHardware.h>

#include "base/sys_info.h"
#include "media/audio/fake_audio_input_stream.h"
#include "media/audio/fake_audio_output_stream.h"
#include "media/audio/mac/audio_input_mac.h"
#include "media/audio/mac/audio_low_latency_output_mac.h"
#include "media/audio/mac/audio_manager_mac.h"
#include "media/audio/mac/audio_output_mac.h"
#include "media/base/limits.h"

static const int kMaxInputChannels = 2;

// Maximum number of output streams that can be open simultaneously.
static const size_t kMaxOutputStreams = 50;

// By experiment the maximum number of audio streams allowed in Leopard
// is 18. But we put a slightly smaller number just to be safe.
static const size_t kMaxOutputStreamsLeopard = 15;

// Initialized to ether |kMaxOutputStreams| or |kMaxOutputStreamsLeopard|.
static size_t g_max_output_streams = 0;

// Returns the number of audio streams allowed. This is a practical limit to
// prevent failure caused by too many audio streams opened.
static size_t GetMaxAudioOutputStreamsAllowed() {
  if (g_max_output_streams == 0) {
    // We are hitting a bug in Leopard where too many audio streams will cause
    // a deadlock in the AudioQueue API when starting the stream. Unfortunately
    // there's no way to detect it within the AudioQueue API, so we put a
    // special hard limit only for Leopard.
    // See bug: http://crbug.com/30242
    int32 major, minor, bugfix;
    base::SysInfo::OperatingSystemVersionNumbers(&major, &minor, &bugfix);
    if (major < 10 || (major == 10 && minor <= 5)) {
      g_max_output_streams = kMaxOutputStreamsLeopard;
    } else {
      // In OS other than OSX Leopard, the number of audio streams
      // allowed is a lot more.
      g_max_output_streams = kMaxOutputStreams;
    }
  }

  return g_max_output_streams;
}

static bool HasAudioHardware(AudioObjectPropertySelector selector) {
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

AudioManagerMac::AudioManagerMac()
    : num_output_streams_(0) {
}

AudioManagerMac::~AudioManagerMac() {
}

bool AudioManagerMac::HasAudioOutputDevices() {
  return HasAudioHardware(kAudioHardwarePropertyDefaultOutputDevice);
}

bool AudioManagerMac::HasAudioInputDevices() {
  return HasAudioHardware(kAudioHardwarePropertyDefaultInputDevice);
}

AudioOutputStream* AudioManagerMac::MakeAudioOutputStream(
    AudioParameters params) {
  if (!params.IsValid())
    return NULL;

  // Limit the number of audio streams opened. This is to prevent using
  // excessive resources for a large number of audio streams. More
  // importantly it prevents instability on certain systems.
  // See bug: http://crbug.com/30242
  if (num_output_streams_ >= GetMaxAudioOutputStreamsAllowed()) {
    return NULL;
  }

  if (params.format == AudioParameters::AUDIO_MOCK) {
    return FakeAudioOutputStream::MakeFakeStream(params);
  } else if (params.format == AudioParameters::AUDIO_PCM_LINEAR) {
    num_output_streams_++;
    return new PCMQueueOutAudioOutputStream(this, params);
  } else if (params.format == AudioParameters::AUDIO_PCM_LOW_LATENCY) {
    num_output_streams_++;
    return new AUAudioOutputStream(this, params);
  }
  return NULL;
}

AudioInputStream* AudioManagerMac::MakeAudioInputStream(
    AudioParameters params) {
  if (!params.IsValid() || (params.channels > kMaxInputChannels))
    return NULL;

  if (params.format == AudioParameters::AUDIO_MOCK) {
    return FakeAudioInputStream::MakeFakeStream(params);
  } else if (params.format == AudioParameters::AUDIO_PCM_LINEAR) {
    return new PCMQueueInAudioInputStream(this, params);
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
void AudioManagerMac::ReleaseOutputStream(AudioOutputStream* stream) {
  DCHECK(stream);
  num_output_streams_--;
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
