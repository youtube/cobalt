// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/voice_isolation_handler.h"

#include <utility>

#include "base/logging.h"
#include "media/base/audio_bus.h"
#include "media/webrtc/voice_isolation/voice_isolation.h"

namespace audio {

VoiceIsolationHandler::VoiceIsolationHandler(
    std::unique_ptr<media::VoiceIsolation> voice_isolation,
    const media::AudioParameters& output_params,
    DeliverProcessedAudioCallback deliver_processed_audio_callback)
    : voice_isolation_(std::move(voice_isolation)),
      deliver_processed_audio_callback_(
          std::move(deliver_processed_audio_callback)),
      output_bus_(media::AudioBus::Create(output_params)) {
  DCHECK(voice_isolation_);
}

VoiceIsolationHandler::~VoiceIsolationHandler() = default;

void VoiceIsolationHandler::ProcessCapturedAudio(
    const media::AudioBus& audio_source,
    base::TimeTicks audio_capture_time,
    std::optional<double> volume,
    const media::AudioGlitchInfo& audio_glitch_info) {
  DCHECK_EQ(output_bus_->channels(), audio_source.channels());
  DCHECK_EQ(output_bus_->frames(), audio_source.frames());
  voice_isolation_->ProcessAudio(audio_source, *output_bus_);
  deliver_processed_audio_callback_.Run(*output_bus_, audio_capture_time,
                                        volume, audio_glitch_info);
}

}  // namespace audio
