// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_buffers_state.h"

#include "base/sync_socket.h"

AudioBuffersState::AudioBuffersState()
    : pending_bytes(0),
      hardware_delay_bytes(0),
      timestamp(base::Time::Now()) {
}

AudioBuffersState::AudioBuffersState(int pending_bytes,
                                     int hardware_delay_bytes)
    : pending_bytes(pending_bytes),
      hardware_delay_bytes(hardware_delay_bytes),
      timestamp(base::Time::Now()) {
}

int AudioBuffersState::total_bytes() {
  return pending_bytes + hardware_delay_bytes;
}

bool AudioBuffersState::Receive(base::SyncSocket *socket) {
  return socket->Receive(this, sizeof(AudioBuffersState)) ==
         sizeof(AudioBuffersState);
}
