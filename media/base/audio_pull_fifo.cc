// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_pull_fifo.h"

#include <algorithm>

#include "base/logging.h"

namespace media {

AudioPullFifo::AudioPullFifo(int channels, int frames, const ReadCB& read_cb)
    : read_cb_(read_cb) {
  fifo_.reset(new AudioFifo(channels, frames));
  bus_ = AudioBus::Create(channels, frames);
}

AudioPullFifo::~AudioPullFifo() {
  read_cb_.Reset();
}

void AudioPullFifo::Consume(AudioBus* destination, int frames_to_consume) {
  DCHECK(destination);
  DCHECK_LE(frames_to_consume, destination->frames());

  int write_pos = 0;
  int remaining_frames_to_provide = frames_to_consume;

  // Try to fulfill the request using what's available in the FIFO.
  ReadFromFifo(destination, &remaining_frames_to_provide, &write_pos);

  // Get the remaining audio frames from the producer using the callback.
  while (remaining_frames_to_provide > 0) {
    // Fill up the FIFO by acquiring audio data from the producer.
    read_cb_.Run(bus_.get());
    fifo_->Push(bus_.get());

    // Try to fulfill the request using what's available in the FIFO.
    ReadFromFifo(destination, &remaining_frames_to_provide, &write_pos);
  }
}

void AudioPullFifo::Clear() {
  fifo_->Clear();
}

void AudioPullFifo::ReadFromFifo(AudioBus* destination,
                                 int* frames_to_provide,
                                 int* write_pos) {
  DCHECK(frames_to_provide);
  DCHECK(write_pos);
  int frames = std::min(fifo_->frames(), *frames_to_provide);
  fifo_->Consume(destination, *write_pos, frames);
  *write_pos += frames;
  *frames_to_provide -= frames;
}

}  // namespace media
