// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_AUDIO_AUDIO_DEBUG_RECORDING_SESSION_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_AUDIO_AUDIO_DEBUG_RECORDING_SESSION_H_

#include "base/macros.h"
#include "third_party/chromium/media/base/media_export.h"

namespace media_m96 {

// Enables/disables audio debug recording on construction/destruction. Objects
// are created using audio::CreateAudioDebugRecordingSession.
class MEDIA_EXPORT AudioDebugRecordingSession {
 public:
  AudioDebugRecordingSession(const AudioDebugRecordingSession&) = delete;
  AudioDebugRecordingSession& operator=(const AudioDebugRecordingSession&) =
      delete;

  virtual ~AudioDebugRecordingSession() = default;

 protected:
  AudioDebugRecordingSession() = default;
};

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_AUDIO_AUDIO_DEBUG_RECORDING_SESSION_H_
