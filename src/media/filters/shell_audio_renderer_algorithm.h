/*
 * Copyright 2012 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MEDIA_FILTERS_SHELL_AUDIO_RENDERER_ALGORITHM_H_
#define MEDIA_FILTERS_SHELL_AUDIO_RENDERER_ALGORITHM_H_

#include "base/callback.h"
#include "media/base/seekable_buffer.h"

namespace media {

class Buffer;

// Converts this object to an abstract interface for a platform-specific
// implementation.
class MEDIA_EXPORT AudioRendererAlgorithm {
 public:
  // returns a platform-specific AudioRendererAlgorithm implementation
  static AudioRendererAlgorithm* Create();

  // Call prior to Initialize() to validate configuration.  Returns false if the
  // configuration is invalid.
  static bool ValidateConfig(int channels,
                             int samples_per_second,
                             int bits_per_channel);

  // Initializes this object with information about the audio stream.
  // |samples_per_second| is in Hz. |read_request_callback| is called to
  // request more data from the client, requests that are fulfilled through
  // calls to EnqueueBuffer().
  virtual void Initialize(int channels,
                          int samples_per_second,
                          int bits_per_channel,
                          float initial_playback_rate,
                          const base::Closure& request_read_cb) = 0;

  // Tries to fill |requested_frames| frames into |dest| with output.
  // Returns the number of frames copied into |dest|.
  // May request more reads via |request_read_cb_| before returning.
  virtual int FillBuffer(uint8* dest, int requested_frames) = 0;

  // Clears internal audio buffers
  virtual void FlushBuffers() = 0;

  // Returns the time of the next byte in our data or kNoTimestamp() if current
  // time is unknown.
  virtual base::TimeDelta GetTime() = 0;

  // Enqueues a buffer. It is called from the owner of the algorithm after a
  // read completes.
  virtual void EnqueueBuffer(Buffer* buffer_in) = 0;

  // we currently don't support playback rates that aren't 0.0 or 1.0
  virtual float playback_rate() const = 0;
  virtual void SetPlaybackRate(float new_rate) = 0;

  // Returns whether the algorithm has enough data at the current playback rate
  // such that it can write data on the next call to FillBuffer().
  virtual bool CanFillBuffer() = 0;

  // Returns true if input buffer is full.
  virtual bool IsQueueFull() = 0;

  // Returns the capacity of input buffer.
  virtual int QueueCapacity() = 0;

  // Increase the capacity of input buffer if possible.
  virtual void IncreaseQueueCapacity() = 0;

  // Returns the number of bytes left in input buffer, which may be larger
  // than QueueCapacity() in the event that a read callback delivered more data
  // than input buffer was intended to hold.
  virtual int bytes_buffered() = 0;

  virtual int bytes_per_frame() = 0;

  virtual int bytes_per_channel() = 0;

  virtual bool is_muted() = 0;

  // NOT part of Chromium's AudioRendererAlgorithm. Added because our algorithm
  // sometimes has to resample to a different sample rate as part of processing.
  virtual int output_sample_rate() = 0;
};

}  // namespace media

#endif  // MEDIA_FILTERS_SHELL_AUDIO_RENDERER_ALGORITHM_H_
