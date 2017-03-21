// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BASE_AUDIO_BLOCK_FIFO_H_
#define COBALT_MEDIA_BASE_AUDIO_BLOCK_FIFO_H_

#include "base/basictypes.h"
#include "base/memory/scoped_vector.h"
#include "cobalt/media/base/audio_bus.h"
#include "cobalt/media/base/media_export.h"

namespace media {

// First-in first-out container for AudioBus elements.
// The FIFO is composed of blocks of AudioBus elements, it accepts interleaved
// data as input and will deinterleave it into the FIFO, and it only allows
// consuming a whole block of AudioBus element.
// This class is thread-unsafe.
class MEDIA_EXPORT AudioBlockFifo {
 public:
  // Creates a new AudioBlockFifo and allocates |blocks| memory, each block
  // of memory can store |channels| of length |frames| data.
  AudioBlockFifo(int channels, int frames, int blocks);
  virtual ~AudioBlockFifo();

  // Pushes interleaved audio data from |source| to the FIFO.
  // The method will deinterleave the data into a audio bus.
  // Push() will crash if the allocated space is insufficient.
  void Push(const void* source, int frames, int bytes_per_sample);

  // Consumes a block of audio from the FIFO.  Returns an AudioBus which
  // contains the consumed audio data to avoid copying.
  // Consume() will crash if the FIFO does not contain a block of data.
  const AudioBus* Consume();

  // Empties the FIFO without deallocating any memory.
  void Clear();

  // Number of available block of memory ready to be consumed in the FIFO.
  int available_blocks() const { return available_blocks_; }

  // Number of available frames of data in the FIFO.
  int GetAvailableFrames() const;

  // Number of unfilled frames in the whole FIFO.
  int GetUnfilledFrames() const;

  // Dynamically increase |blocks| of memory to the FIFO.
  void IncreaseCapacity(int blocks);

 private:
  // The actual FIFO is a vector of audio buses.
  ScopedVector<AudioBus> audio_blocks_;

  // Number of channels in AudioBus.
  const int channels_;

  // Maximum number of frames of data one block of memory can contain.
  // This value is set by |frames| in the constructor.
  const int block_frames_;

  // Used to keep track which block of memory to be written.
  int write_block_;

  // Used to keep track which block of memory to be consumed.
  int read_block_;

  // Number of available blocks of memory to be consumed.
  int available_blocks_;

  // Current write position in the current written block.
  int write_pos_;

  DISALLOW_COPY_AND_ASSIGN(AudioBlockFifo);
};

}  // namespace media

#endif  // COBALT_MEDIA_BASE_AUDIO_BLOCK_FIFO_H_
