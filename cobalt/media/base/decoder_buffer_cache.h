// Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef COBALT_MEDIA_BASE_DECODER_BUFFER_CACHE_H_
#define COBALT_MEDIA_BASE_DECODER_BUFFER_CACHE_H_

#include <deque>

#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "base/time.h"
#include "cobalt/media/base/decoder_buffer.h"
#include "cobalt/media/base/demuxer_stream.h"

namespace cobalt {
namespace media {

// This class can be used to hold media buffers in decoding order.  It also
// provides function that given a media time, the function will find a key frame
// before the media time and clear all buffers before the key frame.  This class
// can be used to "replay" the video from the current playback position and can
// be useful to implement suspend/resume.
class DecoderBufferCache {
 public:
  DecoderBufferCache();

  void AddBuffer(DemuxerStream::Type type,
                 const scoped_refptr<DecoderBuffer>& buffer);
  void ClearSegmentsBeforeMediaTime(base::TimeDelta media_time);
  void ClearAll();

  // Start resuming, reset indices to audio/video buffers to the very beginning.
  void StartResuming();
  scoped_refptr<DecoderBuffer> GetBuffer(DemuxerStream::Type type) const;
  void AdvanceToNextBuffer(DemuxerStream::Type type);

 private:
  typedef std::deque<scoped_refptr<DecoderBuffer> > Buffers;
  typedef std::deque<base::TimeDelta> KeyFrameTimestamps;

  static size_t ClearSegmentsBeforeMediaTime(
      base::TimeDelta media_time, Buffers* buffers,
      KeyFrameTimestamps* key_frame_timestamps);

  base::ThreadChecker thread_checker_;

  Buffers audio_buffers_;
  KeyFrameTimestamps audio_key_frame_timestamps_;

  Buffers video_buffers_;
  KeyFrameTimestamps video_key_frame_timestamps_;

  size_t audio_buffer_index_;
  size_t video_buffer_index_;
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_DECODER_BUFFER_CACHE_H_
