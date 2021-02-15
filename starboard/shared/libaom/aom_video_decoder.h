// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_LIBAOM_AOM_VIDEO_DECODER_H_
#define STARBOARD_SHARED_LIBAOM_AOM_VIDEO_DECODER_H_

#include <aom/aom_codec.h>

#include <queue>
#include <string>

#include "starboard/common/ref_counted.h"
#include "starboard/media.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/filter/cpu_video_frame.h"
#include "starboard/shared/starboard/player/filter/video_decoder_internal.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/shared/starboard/player/job_queue.h"
#include "starboard/shared/starboard/player/job_thread.h"

namespace starboard {
namespace shared {
namespace aom {

class VideoDecoder : public starboard::player::filter::VideoDecoder,
                     private starboard::player::JobQueue::JobOwner {
 public:
  VideoDecoder(SbMediaVideoCodec video_codec,
               SbPlayerOutputMode output_mode,
               SbDecodeTargetGraphicsContextProvider*
                   decode_target_graphics_context_provider);
  ~VideoDecoder() override;

  void Initialize(const DecoderStatusCB& decoder_status_cb,
                  const ErrorCB& error_cb) override;
  size_t GetPrerollFrameCount() const override { return 8; }
  SbTime GetPrerollTimeout() const override { return kSbTimeMax; }
  size_t GetMaxNumberOfCachedFrames() const override { return 12; }

  void WriteInputBuffer(
      const scoped_refptr<InputBuffer>& input_buffer) override;
  void WriteEndOfStream() override;
  void Reset() override;

 private:
  typedef ::starboard::shared::starboard::player::filter::CpuVideoFrame
      CpuVideoFrame;

  void ReportError(const std::string& error_message);

  // The following four functions are only called on the decoder thread except
  // that TeardownCodec() can also be called on other threads when the decoder
  // thread is not running.
  void InitializeCodec();
  void TeardownCodec();
  void DecodeOneBuffer(const scoped_refptr<InputBuffer>& input_buffer);
  void DecodeEndOfStream();

  SbDecodeTarget GetCurrentDecodeTarget() override;

  void UpdateDecodeTarget_Locked(const scoped_refptr<CpuVideoFrame>& frame);

  // The following callbacks will be initialized in Initialize() and won't be
  // changed during the life time of this class.
  DecoderStatusCB decoder_status_cb_;
  ErrorCB error_cb_;

  int current_frame_width_;
  int current_frame_height_;
  scoped_ptr<aom_codec_ctx_t> context_;

  bool stream_ended_;
  bool error_occurred_;

  // Working thread to avoid lengthy decoding work block the player thread.
  scoped_ptr<starboard::player::JobThread> decoder_thread_;

  // Decode-to-texture related state.
  SbPlayerOutputMode output_mode_;

  SbDecodeTargetGraphicsContextProvider*
      decode_target_graphics_context_provider_;

  // If decode-to-texture is enabled, then we store the decode target texture
  // inside of this |decode_target_| member.
  SbDecodeTarget decode_target_;

  // GetCurrentDecodeTarget() needs to be called from an arbitrary thread
  // to obtain the current decode target (which ultimately ends up being a
  // copy of |decode_target_|), we need to safe-guard access to |decode_target_|
  // and we do so through this mutex.
  Mutex decode_target_mutex_;

  std::queue<scoped_refptr<CpuVideoFrame>> frames_;
};

}  // namespace aom
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_LIBAOM_AOM_VIDEO_DECODER_H_
