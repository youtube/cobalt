// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_WEBRTC_VOICE_ISOLATION_VOICE_ISOLATION_H_
#define MEDIA_WEBRTC_VOICE_ISOLATION_VOICE_ISOLATION_H_

#include "base/component_export.h"

namespace media {

class AudioBus;

class COMPONENT_EXPORT(MEDIA_WEBRTC) VoiceIsolation {
 public:
  VoiceIsolation();
  ~VoiceIsolation();

  // Processes audio from input_bus to output_bus.
  void ProcessAudio(const AudioBus& input_bus, AudioBus& output_bus);
};

}  // namespace media

#endif  // MEDIA_WEBRTC_VOICE_ISOLATION_VOICE_ISOLATION_H_
