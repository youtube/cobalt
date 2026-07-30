// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/voice_isolation/voice_isolation.h"

#include "media/base/audio_bus.h"

namespace media {

VoiceIsolation::VoiceIsolation() = default;

VoiceIsolation::~VoiceIsolation() = default;

void VoiceIsolation::ProcessAudio(const AudioBus& input_bus,
                                  AudioBus& output_bus) {
  input_bus.CopyTo(&output_bus);
}

}  // namespace media
