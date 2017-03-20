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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_VIDEO_RENDERER_IMPL_INTERNAL_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_VIDEO_RENDERER_IMPL_INTERNAL_H_

#include <list>

#include "starboard/common/scoped_ptr.h"
#include "starboard/log.h"
#include "starboard/media.h"
#include "starboard/mutex.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/filter/video_decoder_internal.h"
#include "starboard/shared/starboard/player/filter/video_renderer_internal.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/shared/starboard/player/video_frame_internal.h"
#include "starboard/shared/starboard/thread_checker.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

class VideoRendererImpl : public VideoRenderer {
 public:
  explicit VideoRendererImpl(scoped_ptr<VideoDecoder> decoder);

  int GetDroppedFrames() const SB_OVERRIDE { return dropped_frames_; }

  void WriteSample(const InputBuffer& input_buffer) SB_OVERRIDE;
  void WriteEndOfStream() SB_OVERRIDE;

  void Seek(SbMediaTime seek_to_pts) SB_OVERRIDE;

  scoped_refptr<VideoFrame> GetCurrentFrame(SbMediaTime media_time) SB_OVERRIDE;

  bool IsEndOfStreamWritten() const SB_OVERRIDE {
    return end_of_stream_written_;
  }
  bool IsEndOfStreamPlayed() const SB_OVERRIDE;
  bool CanAcceptMoreData() const SB_OVERRIDE;
  bool IsSeekingInProgress() const SB_OVERRIDE;

#if SB_API_VERSION >= SB_PLAYER_DECODE_TO_TEXTURE_API_VERSION
  SbDecodeTarget GetCurrentDecodeTarget() SB_OVERRIDE;
#endif  // SB_API_VERSION >= SB_PLAYER_DECODE_TO_TEXTURE_API_VERSION

 private:
  typedef std::list<scoped_refptr<VideoFrame> > Frames;

  // Preroll considered finished after either kPrerollFrames is cached or EOS
  // is reached.
  static const size_t kPrerollFrames = 8;
  // Set a soft limit for the max video frames we can cache so we can:
  // 1. Avoid using too much memory.
  // 2. Have the frame cache full to simulate the state that the renderer can
  //    no longer accept more data.
  static const size_t kMaxCachedFrames = 12;

  // VideoDecoder::Host method.
  void OnDecoderStatusUpdate(VideoDecoder::Status status,
                             const scoped_refptr<VideoFrame>& frame)
      SB_OVERRIDE;

  ThreadChecker thread_checker_;
  ::starboard::Mutex mutex_;

  bool seeking_;
  Frames frames_;

  // During seeking, all frames inside |frames_| will be cleared but the app
  // should still display the last frame it is rendering.  This frame will be
  // kept inside |last_displayed_frame_|.  It is an empty/black frame before the
  // video is started.
  scoped_refptr<VideoFrame> last_displayed_frame_;

  SbMediaTime seeking_to_pts_;
  bool end_of_stream_written_;
  bool need_more_input_;
  int dropped_frames_;

  scoped_ptr<VideoDecoder> decoder_;
};

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_VIDEO_RENDERER_IMPL_INTERNAL_H_
