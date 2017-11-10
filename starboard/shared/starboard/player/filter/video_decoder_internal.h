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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_VIDEO_DECODER_INTERNAL_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_VIDEO_DECODER_INTERNAL_H_

#include <functional>

#include "starboard/common/ref_counted.h"
#include "starboard/configuration.h"
#include "starboard/player.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/filter/video_frame_internal.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/shared/starboard/player/job_queue.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

// This class decodes encoded video stream into video frames.
class VideoDecoder {
 public:
  typedef ::starboard::shared::starboard::player::InputBuffer InputBuffer;
  typedef ::starboard::shared::starboard::player::filter::VideoFrame VideoFrame;

  enum Status { kNeedMoreInput, kBufferFull, kReleaseAllFrames };

  // |frame| can contain a decoded frame or be NULL.   The user of the class
  // should only call WriteInputFrame() when |status| is |kNeedMoreInput| or
  // when the instance is just created.  Also note that calling Reset() or dtor
  // from this callback *will* result in deadlock.
  typedef std::function<void(Status status,
                             const scoped_refptr<VideoFrame>& frame)>
      DecoderStatusCB;
  typedef std::function<void()> ErrorCB;

  virtual ~VideoDecoder() {}

  virtual void Initialize(const DecoderStatusCB& decoder_status_cb,
                          const ErrorCB& error_cb) = 0;

  // Returns the number of frames the VideoRenderer has to cache before preroll
  // is considered to be complete.
  virtual size_t GetPrerollFrameCount() const = 0;

  // Returns the timeout that preroll is considered to be finished.  Once the
  // first frame is decoded and the timeout has passed, the preroll will be
  // considered as finished even if there isn't enough frames decoded as
  // suggested by GetPrerollFrameCount().
  // On most platforms this can be simply set to |kSbTimeMax|.
  virtual SbTime GetPrerollTimeout() const = 0;

  // Send encoded video frame stored in |input_buffer| to decode.
  virtual void WriteInputBuffer(
      const scoped_refptr<InputBuffer>& input_buffer) = 0;
  // Note that there won't be more input data unless Reset() is called.
  // OnDecoderStatusUpdate will still be called on Host during flushing until
  // the |frame| is an EOS frame.
  virtual void WriteEndOfStream() = 0;
  // Clear any cached buffer of the codec and reset the state of the codec.
  // This function will be called during seek to ensure that there is no left
  // over data from previous buffers.  No DecoderStatusCB call will be made
  // after this function returns unless WriteInputFrame() or WriteEndOfStream()
  // is called again.
  virtual void Reset() = 0;

  // May be called from an arbitrary thread (e.g. a renderer thread).
  virtual SbDecodeTarget GetCurrentDecodeTarget() = 0;

  // Individual implementations must implement this function to indicate which
  // output modes they support.
  static bool OutputModeSupported(SbPlayerOutputMode output_mode,
                                  SbMediaVideoCodec codec,
                                  SbDrmSystem drm_system);
};

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_VIDEO_DECODER_INTERNAL_H_
