// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_SPLICER_H_
#define MEDIA_BASE_AUDIO_SPLICER_H_

#include <deque>

#include "base/memory/ref_counted.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/media_export.h"

namespace media {

class AudioDecoderConfig;
class Buffer;

// Helper class that handles filling gaps and resolving overlaps.
class MEDIA_EXPORT AudioSplicer {
 public:
  AudioSplicer(int bytes_per_frame, int samples_per_second);
  ~AudioSplicer();

  // Resets the splicer state by clearing the output buffers queue,
  // and resetting the timestamp helper.
  void Reset();

  // Adds a new buffer full of samples or end of stream buffer to the splicer.
  // Returns true if the buffer was accepted. False is returned if an error
  // occurred.
  bool AddInput(const scoped_refptr<Buffer>& input);

  // Returns true if the splicer has a buffer to return.
  bool HasNextBuffer() const;

  // Removes the next buffer from the output buffer queue and returns it.
  // This should only be called if HasNextBuffer() returns true.
  scoped_refptr<Buffer> GetNextBuffer();

 private:
  void AddOutputBuffer(const scoped_refptr<Buffer>& buffer);

  AudioTimestampHelper output_timestamp_helper_;

  // Minimum gap size needed before the splicer will take action to
  // fill a gap. This avoids periodically inserting and then dropping samples
  // when the buffer timestamps are slightly off because of timestamp rounding
  // in the source content.
  int min_gap_size_;

  std::deque<scoped_refptr<Buffer> > output_buffers_;
  bool received_end_of_stream_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(AudioSplicer);
};

}  // namespace media

#endif
