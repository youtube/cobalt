// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BASE_AUDIO_BUFFER_QUEUE_H_
#define COBALT_MEDIA_BASE_AUDIO_BUFFER_QUEUE_H_

#include <deque>

#include "base/basictypes.h"
#include "cobalt/media/base/audio_buffer.h"
#include "cobalt/media/base/media_export.h"

namespace media {

class AudioBus;

// A queue of AudioBuffers to support reading of arbitrary chunks of a media
// data source. Audio data can be copied into an AudioBus for output. The
// current position can be forwarded to anywhere in the buffered data.
//
// This class is not inherently thread-safe. Concurrent access must be
// externally serialized.
class MEDIA_EXPORT AudioBufferQueue {
 public:
  AudioBufferQueue();
  ~AudioBufferQueue();

  // Clears the buffer queue.
  void Clear();

  // Appends |buffer_in| to this queue.
  void Append(const scoped_refptr<AudioBuffer>& buffer_in);

  // Reads a maximum of |frames| frames into |dest| from the current position.
  // Returns the number of frames read. The current position will advance by the
  // amount of frames read. |dest_frame_offset| specifies a starting offset into
  // |dest|. On each call, the frames are converted from their source format
  // into the destination AudioBus.
  int ReadFrames(int frames, int dest_frame_offset, AudioBus* dest);

  // Copies up to |frames| frames from current position to |dest|. Returns
  // number of frames copied. Doesn't advance current position. Starts at
  // |source_frame_offset| from current position. |dest_frame_offset| specifies
  // a starting offset into |dest|. On each call, the frames are converted from
  // their source format into the destination AudioBus.
  int PeekFrames(int frames, int source_frame_offset, int dest_frame_offset,
                 AudioBus* dest);

  // Moves the current position forward by |frames| frames. If |frames| exceeds
  // frames available, the seek operation will fail.
  void SeekFrames(int frames);

  // Returns the number of frames buffered beyond the current position.
  int frames() const { return frames_; }

 private:
  // Definition of the buffer queue.
  typedef std::deque<scoped_refptr<AudioBuffer> > BufferQueue;

  // An internal method shared by ReadFrames() and SeekFrames() that actually
  // does reading. It reads a maximum of |frames| frames into |dest|. Returns
  // the number of frames read. The current position will be moved forward by
  // the number of frames read if |advance_position| is set. If |dest| is NULL,
  // only the current position will advance but no data will be copied.
  // |source_frame_offset| can be used to skip frames before reading.
  // |dest_frame_offset| specifies a starting offset into |dest|.
  int InternalRead(int frames, bool advance_position, int source_frame_offset,
                   int dest_frame_offset, AudioBus* dest);

  BufferQueue::iterator current_buffer_;
  BufferQueue buffers_;
  int current_buffer_offset_;

  // Number of frames available to be read in the buffer.
  int frames_;

  DISALLOW_COPY_AND_ASSIGN(AudioBufferQueue);
};

}  // namespace media

#endif  // COBALT_MEDIA_BASE_AUDIO_BUFFER_QUEUE_H_
