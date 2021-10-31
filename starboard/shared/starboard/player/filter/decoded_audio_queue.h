// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Modifications Copyright 2017 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_DECODED_AUDIO_QUEUE_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_DECODED_AUDIO_QUEUE_H_

#include <deque>

#include "starboard/configuration.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/decoded_audio_internal.h"
#include "starboard/types.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

// A queue of AudioBuffers to support reading of arbitrary chunks of a media
// data source. Audio data can be copied into an DecodedAudio for output. The
// current position can be forwarded to anywhere in the buffered data.
//
// This class is not inherently thread-safe. Concurrent access must be
// externally serialized.
class DecodedAudioQueue {
 public:
  DecodedAudioQueue();
  ~DecodedAudioQueue();

  // Clears the buffer queue.
  void Clear();

  // Appends |decoded_audio| to this queue.
  void Append(const scoped_refptr<DecodedAudio>& decoded_audio);

  // Reads a maximum of |frames| frames into |dest| from the current position.
  // Returns the number of frames read. The current position will advance by the
  // amount of frames read. |dest_frame_offset| specifies a starting offset into
  // |dest|. On each call, the frames are converted from their source format
  // into the destination DecodedAudio.
  int ReadFrames(int frames, int dest_frame_offset, DecodedAudio* dest);

  // Copies up to |frames| frames from current position to |dest|. Returns
  // number of frames copied. Doesn't advance current position. Starts at
  // |source_frame_offset| from current position. |dest_frame_offset| specifies
  // a starting offset into |dest|. On each call, the frames are converted from
  // their source format into the destination DecodedAudio.
  int PeekFrames(int frames,
                 int source_frame_offset,
                 int dest_frame_offset,
                 DecodedAudio* dest);

  // Moves the current position forward by |frames| frames. If |frames| exceeds
  // frames available, the seek operation will fail.
  void SeekFrames(int frames);

  // Returns the number of frames buffered beyond the current position.
  int frames() const { return frames_; }

 private:
  // Definition of the buffer queue.
  typedef std::deque<scoped_refptr<DecodedAudio> > BufferQueue;

  // An internal method shared by ReadFrames() and SeekFrames() that actually
  // does reading. It reads a maximum of |frames| frames into |dest|. Returns
  // the number of frames read. The current position will be moved forward by
  // the number of frames read if |advance_position| is set. If |dest| is NULL,
  // only the current position will advance but no data will be copied.
  // |source_frame_offset| can be used to skip frames before reading.
  // |dest_frame_offset| specifies a starting offset into |dest|.
  int InternalRead(int frames,
                   bool advance_position,
                   int source_frame_offset,
                   int dest_frame_offset,
                   DecodedAudio* dest);

  BufferQueue::iterator current_buffer_;
  BufferQueue buffers_;
  int current_buffer_offset_;

  // Number of frames available to be read in the buffer.
  int frames_;

  DecodedAudioQueue(const DecodedAudioQueue&) = delete;
  void operator=(const DecodedAudioQueue&) = delete;
};

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_DECODED_AUDIO_QUEUE_H_
