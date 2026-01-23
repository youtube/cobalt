// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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
#include <limits>

#include "starboard/common/ref_counted.h"
#include "starboard/configuration.h"
#include "starboard/player.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/filter/common.h"
#include "starboard/shared/starboard/player/filter/video_frame_internal.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"

namespace starboard {

// This class decodes encoded video stream into video frames.
class VideoDecoder {
 public:
  enum Status {
    kNeedMoreInput,    // Signals that more input is required to produce output.
    kBufferFull,       // Signals that the decoder can no longer accept input.
    kReleaseAllFrames  // Signals that all frames decoded should be released.
                       // This can only happen during Reset() or dtor.
  };

  // |frame| can contain a decoded frame or be NULL.  It has to be NULL when
  // |status| is kReleaseAllFrames.  The user of the class should only call
  // WriteInputFrame() when |status| is |kNeedMoreInput| or when the instance
  // is just created or Reset() is just called.
  // When |status| is |kNeedMoreInput|, it cannot become |kBufferFull| unless
  // WriteInputBuffer() or WriteEndOfStream() is called.
  // Also note that calling Reset() or dtor from this callback *will* result in
  // deadlock.
  using DecoderStatusCB =
      std::function<void(Status status,
                         const scoped_refptr<VideoFrame>& frame)>;

  virtual ~VideoDecoder() {}

  // This function has to be called before any other functions to setup the
  // stage.
  virtual void Initialize(DecoderStatusCB decoder_status_cb,
                          ErrorCB error_cb) = 0;

  // Returns the number of frames the VideoRenderer has to cache before preroll
  // is considered to be complete.
  virtual size_t GetPrerollFrameCount() const = 0;

  // Returns the timeout (in microseconds) that preroll is considered to be
  // finished.  Once the first frame is decoded and the timeout has passed, the
  // preroll will be considered as finished even if there isn't enough frames
  // decoded as suggested by GetPrerollFrameCount().
  // On most platforms this can be simply set to
  // |std::numeric_limits<int64_t>::max()|.
  virtual int64_t GetPrerollTimeout() const = 0;

  // Returns a soft limit of the maximum number of frames the user of this class
  // (usually the VideoRenderer) should cache, i.e. the user won't write more
  // inputs once the frames it holds on exceed this limit.  Note that other part
  // of the video pipeline like the VideoRendererSink may also cache frames.  It
  // is the responsibility of the decoder to ensure that this wouldn't result in
  // anything catastrophic.
  // Also it is worth noting that the return value should always greater than 1
  // for the video renderer to work properly.  It should be at least 4 for
  // acceptable playback performance.  A number greater than 6 is recommended.
  virtual size_t GetMaxNumberOfCachedFrames() const = 0;

  // Send encoded video frames stored in |input_buffers| to decode.
  virtual void WriteInputBuffers(const InputBuffers& input_buffers) = 0;

  // Note that there won't be more input data unless Reset() is called.
  // |decoder_status_cb| can still be called multiple times afterwards to
  // ensure that all remaining frames are returned until the |frame| is an EOS
  // frame.
  virtual void WriteEndOfStream() = 0;

  // Clear any cached buffer of the codec and reset the state of the codec.
  // This function will be called during seek to ensure that there is no left
  // over data from previous buffers.  |decoder_status_cb| won't be called
  // after this function returns unless WriteInputFrame() or WriteEndOfStream()
  // is called again.
  virtual void Reset() = 0;

  // This function can only be called when the current SbPlayerOutputMode is
  // |kSbPlayerOutputModeDecodeToTexture|.  It has to return valid value after
  // the |decoder_status_cb| is called with a valid frame for the first time
  // unless Reset() is called.
  // May be called from an arbitrary thread (e.g. a renderer thread).
  virtual SbDecodeTarget GetCurrentDecodeTarget() = 0;
};

}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_VIDEO_DECODER_INTERNAL_H_
