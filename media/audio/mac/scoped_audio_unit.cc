// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/mac/scoped_audio_unit.h"

#include "base/mac/mac_logging.h"

namespace media {

constexpr AudioComponentDescription desc = {kAudioUnitType_Output,
                                            kAudioUnitSubType_HALOutput,
                                            kAudioUnitManufacturer_Apple, 0, 0};

static void DestroyAudioUnit(AudioUnit audio_unit) {
  OSStatus result = AudioUnitUninitialize(audio_unit);
  OSSTATUS_DLOG_IF(ERROR, result != noErr, result)
      << "AudioUnitUninitialize() failed : " << audio_unit;
  result = AudioComponentInstanceDispose(audio_unit);
  OSSTATUS_DLOG_IF(ERROR, result != noErr, result)
      << "AudioComponentInstanceDispose() failed : " << audio_unit;
}

ScopedAudioUnit::ScopedAudioUnit(AudioDeviceID device, AUElement element) {
  AudioComponent comp = AudioComponentFindNext(0, &desc);
  if (!comp)
    return;

  AudioUnit audio_unit;
  OSStatus result = AudioComponentInstanceNew(comp, &audio_unit);
  if (result != noErr) {
    OSSTATUS_DLOG(ERROR, result) << "AudioComponentInstanceNew() failed.";
    return;
  }

  const UInt32 enable_input_io = element == AUElement::INPUT ? 1 : 0;
  result = AudioUnitSetProperty(audio_unit, kAudioOutputUnitProperty_EnableIO,
                                kAudioUnitScope_Input, AUElement::INPUT,
                                &enable_input_io, sizeof(enable_input_io));
  if (result != noErr) {
    OSSTATUS_DLOG(ERROR, result)
        << "Failed to set input enable IO for audio unit.";
    DestroyAudioUnit(audio_unit);
    return;
  }

  const UInt32 enable_output_io = !enable_input_io;
  result = AudioUnitSetProperty(audio_unit, kAudioOutputUnitProperty_EnableIO,
                                kAudioUnitScope_Output, AUElement::OUTPUT,
                                &enable_output_io, sizeof(enable_output_io));
  if (result != noErr) {
    OSSTATUS_DLOG(ERROR, result)
        << "Failed to set output enable IO for audio unit.";
    DestroyAudioUnit(audio_unit);
    return;
  }

  result = AudioUnitSetProperty(
      audio_unit, kAudioOutputUnitProperty_CurrentDevice,
      kAudioUnitScope_Global, 0, &device, sizeof(AudioDeviceID));
  if (result != noErr) {
    OSSTATUS_DLOG(ERROR, result)
        << "Failed to set current device for audio unit.";
    DestroyAudioUnit(audio_unit);
    return;
  }

  audio_unit_ = audio_unit;
}

ScopedAudioUnit::~ScopedAudioUnit() {
  if (audio_unit_)
    DestroyAudioUnit(audio_unit_);
}

}  // namespace media
