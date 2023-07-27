// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPEECH_AUDIO_BUFFER_H_
#define CONTENT_BROWSER_SPEECH_AUDIO_BUFFER_H_

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"

namespace content {

// Models a chunk derived from an AudioBuffer.
class CONTENT_EXPORT AudioChunk
    : public base::RefCountedThreadSafe<AudioChunk> {
 public:
  explicit AudioChunk(int bytes_per_sample);
  // Creates a chunk of |length| bytes, initialized to zeros.
  AudioChunk(size_t length, int bytes_per_sample);
  AudioChunk(const uint8_t* data, size_t length, int bytes_per_sample);

  bool IsEmpty() const;
  int bytes_per_sample() const { return bytes_per_sample_; }
  size_t NumSamples() const;
  const std::string& AsString() const;
  int16_t GetSample16(size_t index) const;
  const int16_t* SamplesData16() const;
  uint8_t* writable_data() {
    return reinterpret_cast<uint8_t*>(&data_string_[0]);
  }

 private:
  friend class base::RefCountedThreadSafe<AudioChunk>;
  ~AudioChunk() {}

  std::string data_string_;
  const int bytes_per_sample_;

  DISALLOW_COPY_AND_ASSIGN(AudioChunk);
};

// Models an audio buffer. The current implementation relies on on-demand
// allocations of AudioChunk(s) (which uses a string as storage).
class AudioBuffer {
 public:
  explicit AudioBuffer(int bytes_per_sample);
  ~AudioBuffer();

  // Enqueues a copy of |length| bytes of |data| buffer.
  void Enqueue(const uint8_t* data, size_t length);

  // Dequeues, in FIFO order, a single chunk respecting the length of the
  // corresponding Enqueue call (in a nutshell: multiple Enqueue calls followed
  // by DequeueSingleChunk calls will return the individual chunks without
  // merging them).
  scoped_refptr<AudioChunk> DequeueSingleChunk();

  // Dequeues all previously enqueued chunks, merging them in a single chunk.
  scoped_refptr<AudioChunk> DequeueAll();

  // Removes and frees all the enqueued chunks.
  void Clear();

  // Checks whether the buffer is empty.
  bool IsEmpty() const;

 private:
  using ChunksContainer = base::circular_deque<scoped_refptr<AudioChunk>>;
  ChunksContainer chunks_;
  const int bytes_per_sample_;

  DISALLOW_COPY_AND_ASSIGN(AudioBuffer);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SPEECH_AUDIO_BUFFER_H_
