// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_WIN32_VIDEO_DECODER_H_
#define STARBOARD_SHARED_WIN32_VIDEO_DECODER_H_

#include "starboard/common/ref_counted.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/configuration.h"
#include "starboard/shared/starboard/player/filter/player_components.h"
#include "starboard/shared/starboard/player/filter/video_decoder_internal.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/shared/starboard/player/job_queue.h"
#include "starboard/shared/starboard/player/video_frame_internal.h"
#include "starboard/shared/starboard/thread_checker.h"
#include "starboard/shared/win32/atomic_queue.h"
#include "starboard/shared/win32/media_common.h"
#include "starboard/shared/win32/video_decoder_thread.h"
#include "starboard/shared/win32/win32_decoder_impl.h"

namespace starboard {
namespace shared {
namespace win32 {

class VideoDecoder
    : public ::starboard::shared::starboard::player::filter::HostedVideoDecoder,
      private ::starboard::shared::starboard::player::JobQueue::JobOwner,
      private VideoDecodedCallback {
 public:
  explicit VideoDecoder(const VideoParameters& params);
  ~VideoDecoder() SB_OVERRIDE;

  void SetHost(Host* host) SB_OVERRIDE;
  void WriteInputBuffer(const scoped_refptr<InputBuffer>& input_buffer)
      SB_OVERRIDE;
  void WriteEndOfStream() SB_OVERRIDE;
  void Reset() SB_OVERRIDE;

  SbDecodeTarget GetCurrentDecodeTarget() SB_OVERRIDE;

  // Implements class VideoDecodedCallback.
  void OnVideoDecoded(VideoFramePtr data) SB_OVERRIDE;

 private:
  // These variables will be initialized inside ctor or SetHost() and will not
  // be changed during the life time of this class.
  const SbMediaVideoCodec video_codec_;
  Host* host_;

  // Decode-to-texture related state.
  SbPlayerOutputMode output_mode_;

  SbDecodeTargetGraphicsContextProvider*
      decode_target_graphics_context_provider_;

  scoped_ptr<AbstractWin32VideoDecoder> impl_;
  AtomicQueue<VideoFramePtr> decoded_data_;
  ::starboard::Mutex mutex_;
  scoped_ptr<VideoDecoderThread> video_decoder_thread_;
  ::starboard::shared::starboard::ThreadChecker thread_checker_;
};

}  // namespace win32
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_WIN32_VIDEO_DECODER_H_
