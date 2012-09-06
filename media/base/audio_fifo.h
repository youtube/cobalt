// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_FIFO_H_
#define MEDIA_BASE_AUDIO_FIFO_H_

#include "media/base/audio_bus.h"
#include "media/base/media_export.h"

namespace media {

// First-in first-out container for AudioBus elements.
// The maximum number of audio frames in the FIFO is set at construction and
// can not be extended dynamically.  The allocated memory is utilized as a
// ring buffer.
class MEDIA_EXPORT AudioFifo {
 public:
  // Creates a new AudioFifo and allocates |channels| of length |frames|.
  AudioFifo(int channels, int frames);
  virtual ~AudioFifo();

  // Pushes all audio channel data from |source| to the FIFO.
  // Returns false if the allocated space is not sufficient.  Partial data is
  // not written to the FIFO for this overflow case.
  bool Push(const AudioBus* source);

  // Consumes |frames_to_consume| audio frames from the FIFO and copies
  // them to |destination|.
  // Returns false if the FIFO does not contain |frames_to_consume| frames
  // or if there is not sufficient space in |destination| to store the frames.
  bool Consume(AudioBus* destination, int frames_to_consume);

  // Empties the FIFO without deallocating any memory.
  void Clear();

  // Number of actual audio frames in the FIFO.
  int frames_in_fifo() const { return frames_in_fifo_; }

 private:
  int max_frames() const { return max_frames_in_fifo_; }

  // The actual FIFO is an audio bus implemented as a ring buffer.
  scoped_ptr<AudioBus> audio_bus_;

  // Maximum number of elements the FIFO can contain.
  // This value is set by |frames| in the constructor.
  const int max_frames_in_fifo_;

  // Number of actual elements in the FIFO.
  int frames_in_fifo_;

  // Current read position.
  int read_pos_;

  // Current write position.
  int write_pos_;

  DISALLOW_COPY_AND_ASSIGN(AudioFifo);
};

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_FIFO_H_
