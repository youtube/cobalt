// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_MAC_SCOPED_AUDIO_UNIT_H_
#define MEDIA_AUDIO_MAC_SCOPED_AUDIO_UNIT_H_

#include <AudioUnit/AudioUnit.h>
#include <CoreAudio/CoreAudio.h>

#include "base/macros.h"

namespace media {

// For whatever reason Apple doesn't have constants defined for these; per the
// documentation, we use bus 0 for output and bus 1 for input:
// http://developer.apple.com/library/mac/#technotes/tn2091/_index.html
enum AUElement : AudioUnitElement { OUTPUT = 0, INPUT = 1 };

// A helper class that ensures AudioUnits are properly disposed of.
class ScopedAudioUnit {
 public:
  // Creates a new AudioUnit and sets its device for |element| to |device|. If
  // the operation fails, is_valid() will return false and audio_unit() will
  // return nullptr.
  ScopedAudioUnit(AudioDeviceID device, AUElement element);

  ScopedAudioUnit(const ScopedAudioUnit&) = delete;
  ScopedAudioUnit& operator=(const ScopedAudioUnit&) = delete;

  ~ScopedAudioUnit();

  bool is_valid() const { return audio_unit_ != nullptr; }
  AudioUnit audio_unit() const { return audio_unit_; }

 private:
  AudioUnit audio_unit_ = nullptr;
};

}  // namespace media

#endif  // MEDIA_AUDIO_MAC_SCOPED_AUDIO_UNIT_H_
