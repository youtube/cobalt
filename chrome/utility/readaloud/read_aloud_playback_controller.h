// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_READALOUD_READ_ALOUD_PLAYBACK_CONTROLLER_H_
#define CHROME_UTILITY_READALOUD_READ_ALOUD_PLAYBACK_CONTROLLER_H_

#include "chrome/common/readaloud/read_aloud.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace readaloud {

// Implements both ReadAloudPlayerFactory (the service entry point) and
// ReadAloudPlayer (the control interface) for simplicity.
class ReadAloudPlaybackController
    : public read_aloud::mojom::ReadAloudPlayerFactory,
      public read_aloud::mojom::ReadAloudPlayer {
 public:
  explicit ReadAloudPlaybackController(
      mojo::PendingReceiver<read_aloud::mojom::ReadAloudPlayerFactory>
          receiver);

  ReadAloudPlaybackController(const ReadAloudPlaybackController&) = delete;
  ReadAloudPlaybackController& operator=(const ReadAloudPlaybackController&) =
      delete;

  ~ReadAloudPlaybackController() override;

 private:
  // read_aloud::mojom::ReadAloudPlayerFactory:
  void CreatePlayer(
      mojo::PendingReceiver<read_aloud::mojom::ReadAloudPlayer> player,
      mojo::PendingRemote<read_aloud::mojom::ReadAloudPlayerObserver> observer)
      override;

  mojo::Receiver<read_aloud::mojom::ReadAloudPlayerFactory> receiver_;
  mojo::Receiver<read_aloud::mojom::ReadAloudPlayer> player_receiver_{this};
  mojo::Remote<read_aloud::mojom::ReadAloudPlayerObserver> observer_;
};

}  // namespace readaloud

#endif  // CHROME_UTILITY_READALOUD_READ_ALOUD_PLAYBACK_CONTROLLER_H_
