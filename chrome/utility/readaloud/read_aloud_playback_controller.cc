// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/readaloud/read_aloud_playback_controller.h"

#include <utility>

namespace readaloud {

ReadAloudPlaybackController::ReadAloudPlaybackController(
    mojo::PendingReceiver<read_aloud::mojom::ReadAloudPlayerFactory> receiver)
    : receiver_(this, std::move(receiver)) {}

ReadAloudPlaybackController::~ReadAloudPlaybackController() = default;

void ReadAloudPlaybackController::CreatePlayer(
    mojo::PendingReceiver<read_aloud::mojom::ReadAloudPlayer> player,
    mojo::PendingRemote<read_aloud::mojom::ReadAloudPlayerObserver> observer) {
  player_receiver_.reset();
  player_receiver_.Bind(std::move(player));
  observer_.reset();
  observer_.Bind(std::move(observer));
}

}  // namespace readaloud
