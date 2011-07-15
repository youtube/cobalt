// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_BUFFERS_STATE_H_
#define MEDIA_AUDIO_AUDIO_BUFFERS_STATE_H_

#include "base/time.h"

// AudioBuffersState struct stores current state of audio buffers along with
// the timestamp of the moment this state corresponds to. It is used for audio
// synchronization.
struct AudioBuffersState {
  AudioBuffersState();
  AudioBuffersState(int pending_bytes, int hardware_delay_bytes);

  int total_bytes();

  // Number of bytes we currently have in our software buffer.
  int pending_bytes;

  // Number of bytes that have been written to the device, but haven't
  // been played yet.
  int hardware_delay_bytes;

  // Timestamp of the moment when the buffers state was captured. It is used
  // to account for the time it takes to deliver AudioBuffersState from
  // the browser process to the renderer.
  base::Time timestamp;
};


#endif  // MEDIA_AUDIO_AUDIO_BUFFERS_STATE_H_
