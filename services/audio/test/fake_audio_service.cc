// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/test/fake_audio_service.h"

namespace audio {

FakeAudioService::FakeAudioService() = default;

FakeAudioService::~FakeAudioService() = default;

void FakeAudioService::BindStreamFactory(
    mojo::PendingReceiver<media::mojom::AudioStreamFactory> receiver) {
  fake_output_stream_.BindReceiver(std::move(receiver));
}

}  // namespace audio
