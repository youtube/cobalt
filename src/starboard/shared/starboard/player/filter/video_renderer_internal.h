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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_VIDEO_RENDERER_INTERNAL_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_VIDEO_RENDERER_INTERNAL_H_

#include "starboard/common/scoped_ptr.h"
#include "starboard/log.h"
#include "starboard/media.h"
#include "starboard/mutex.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/filter/video_decoder_internal.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/shared/starboard/player/video_frame_internal.h"
#include "starboard/shared/starboard/thread_checker.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

class VideoRenderer : protected VideoDecoder::Host {
 public:
  virtual ~VideoRenderer() {}

  virtual int GetDroppedFrames() const = 0;
  virtual void WriteSample(const InputBuffer& input_buffer) = 0;
  virtual void WriteEndOfStream() = 0;
  virtual void Seek(SbMediaTime seek_to_pts) = 0;
  virtual scoped_refptr<VideoFrame> GetCurrentFrame(SbMediaTime media_time) = 0;
  virtual bool IsEndOfStreamWritten() const = 0;
  virtual bool IsEndOfStreamPlayed() const = 0;
  virtual bool CanAcceptMoreData() const = 0;
  virtual bool IsSeekingInProgress() const = 0;
#if SB_API_VERSION >= SB_PLAYER_DECODE_TO_TEXTURE_API_VERSION
  virtual SbDecodeTarget GetCurrentDecodeTarget() = 0;
#endif  // SB_API_VERSION >= SB_PLAYER_DECODE_TO_TEXTURE_API_VERSION

  // Individual implementation has to implement this function to create a
  // VideoRenderer.
  static scoped_ptr<VideoRenderer> Create(
      scoped_ptr<VideoDecoder> video_decoder);
};

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_VIDEO_RENDERER_INTERNAL_H_
