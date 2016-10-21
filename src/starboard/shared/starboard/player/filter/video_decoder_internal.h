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

#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/shared/starboard/player/video_frame_internal.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

// This class decodes encoded video stream into video frames.
class VideoDecoder {
 public:
  enum Status { kNeedMoreInput, kBufferFull, kFatalError };

  class Host {
   public:
    // |frame| can contain a decoded frame or be NULL when |status| is not
    // kFatalError.   When status is kFatalError, |frame| will be NULL.  Its
    // user should only call WriteInputFrame() when |status| is kNeedMoreInput
    // or when the instance is just created.  Also note that calling Reset() or
    // dtor from this callback will result in deadlock.
    virtual void OnDecoderStatusUpdate(Status status, VideoFrame* frame) = 0;

   protected:
    ~Host() {}
  };

  virtual ~VideoDecoder() {}

  virtual void SetHost(Host* host) = 0;

  // Send encoded video frame stored in |input_buffer| to decode.
  virtual void WriteInputBuffer(const InputBuffer& input_buffer) = 0;
  // Note that there won't be more input data unless Reset() is called.
  // OnDecoderStatusUpdate will still be called on Host during flushing until
  // the |frame| is an EOS frame.
  virtual void WriteEndOfStream() = 0;
  // Clear any cached buffer of the codec and reset the state of the codec.
  // This function will be called during seek to ensure that there is no left
  // over data from previous buffers.  No DecoderStatusFunc call will be made
  // after this function returns unless WriteInputFrame() or WriteEndOfStream()
  // is called again.
  virtual void Reset() = 0;

  // Individual implementation has to implement this function to create a video
  // decoder.
  static VideoDecoder* Create(SbMediaVideoCodec video_codec);
};

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_VIDEO_DECODER_INTERNAL_H_
