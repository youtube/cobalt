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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_VIDEO_RENDERER_INTERNAL_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_VIDEO_RENDERER_INTERNAL_H_

#include <vector>

#include "starboard/log.h"
#include "starboard/media.h"
#include "starboard/mutex.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/shared/starboard/player/video_decoder_internal.h"
#include "starboard/shared/starboard/player/video_frame_internal.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {

class VideoRenderer : private VideoDecoder::Host {
 public:
  explicit VideoRenderer(VideoDecoder* decoder);
  ~VideoRenderer();

  bool is_valid() const { return true; }

  void WriteSample(const InputBuffer& input_buffer);
  void WriteEndOfStream();

  void Seek(SbMediaTime seek_to_pts);

  const VideoFrame& GetCurrentFrame(SbMediaTime media_time);

  bool IsEndOfStreamWritten() const { return end_of_stream_written_; }
  bool IsEndOfStreamPlayed() const;
  bool CanAcceptMoreData() const;
  bool IsSeekingInProgress() const;

 private:
  typedef std::vector<VideoFrame> Frames;

  // Preroll considered finished after either kPrerollFrames is cached or EOS
  // is reached.
  static const size_t kPrerollFrames = 1;
  // Set a soft limit for the max video frames we can cache so we can:
  // 1. Avoid using too much memory.
  // 2. Have the frame cache full to simulate the state that the renderer can
  //    no longer accept more data.
  static const size_t kMaxCachedFrames = 8;

  // VideoDecoder::Host method.
  void OnDecoderStatusUpdate(VideoDecoder::Status status,
                             VideoFrame* frame) SB_OVERRIDE;

  ::starboard::Mutex mutex_;

  bool seeking_;
  Frames frames_;

  VideoFrame empty_frame_;

  SbMediaTime seeking_to_pts_;
  bool end_of_stream_written_;
  bool need_more_input_;

  VideoDecoder* decoder_;
};

}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_VIDEO_RENDERER_INTERNAL_H_
