// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/readaloud/read_aloud_audio_renderer.h"

#include <utility>

namespace readaloud {

ReadAloudAudioRenderer::ReadAloudAudioRenderer(
    mojo::PendingReceiver<read_aloud::mojom::ReadAloudPlayerFactory> receiver)
    : receiver_(this, std::move(receiver)) {}

ReadAloudAudioRenderer::~ReadAloudAudioRenderer() = default;

void ReadAloudAudioRenderer::CreatePlayer(
    mojo::PendingReceiver<read_aloud::mojom::ReadAloudPlayer> player,
    mojo::PendingRemote<read_aloud::mojom::ReadAloudPlayerObserver> observer) {
  player_receiver_.reset();
  // TODO(b/524283143): Remove this check once ReadAloudPlayer is no longer
  // work-in-progress and guaranteed to be non-nullable.
  if (player.is_valid()) {
    player_receiver_.Bind(std::move(player));
  }
  observer_.reset();
  // TODO(b/524283143): Remove this check once ReadAloudPlayerObserver is no
  // longer work-in-progress and guaranteed to be non-nullable.
  if (observer.is_valid()) {
    observer_.Bind(std::move(observer));
  }
}

}  // namespace readaloud
