// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_LIBDAV1D_DAV1D_VIDEO_DECODER_H_
#define STARBOARD_SHARED_LIBDAV1D_DAV1D_VIDEO_DECODER_H_

#include <limits>
#include <memory>
#include <mutex>
#include <queue>
#include <string>

#include "starboard/common/ref_counted.h"
#include "starboard/common/size.h"
#include "starboard/decode_target.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/filter/cpu_video_frame.h"
#include "starboard/shared/starboard/player/filter/video_decoder_internal.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/shared/starboard/player/job_thread.h"
#include "third_party/dav1d/libdav1d/include/dav1d/dav1d.h"

namespace starboard {

class Dav1dVideoDecoder : public VideoDecoder, private JobQueue::JobOwner {
 public:
  Dav1dVideoDecoder(SbMediaVideoCodec video_codec,
                    SbPlayerOutputMode output_mode,
                    SbDecodeTargetGraphicsContextProvider*
                        decode_target_graphics_context_provider,
                    bool may_reduce_quality_for_speed);
  ~Dav1dVideoDecoder() override;

  void Initialize(DecoderStatusCB decoder_status_cb, ErrorCB error_cb) override;

  // TODO: Verify if these values are correct.
  size_t GetPrerollFrameCount() const override { return 8; }
  int64_t GetPrerollTimeout() const override {
    return std::numeric_limits<int64_t>::max();
  }
  size_t GetMaxNumberOfCachedFrames() const override { return 12; }

  void WriteInputBuffers(const InputBuffers& input_buffers) override;
  void WriteEndOfStream() override;
  void Reset() override;

 private:
  const int kDav1dSuccess = 0;

  const int kDav1dPlaneY = 0;
  const int kDav1dPlaneU = 1;
  const int kDav1dPlaneV = 2;

  void ReportError(const std::string& error_message);

  // The following functions are only called on the decoder thread.
  void InitializeCodec();
  void TeardownCodec();
  void DecodeOneBuffer(const scoped_refptr<InputBuffer>& input_buffer);
  void DecodeEndOfStream(int64_t timeout);

  // Sends all available decoded frames from dav1d for outputting. Returns the
  // number of frames decoded. The output argument tracks whether an
  // unrecoverable error occurred in dav1d while trying to output frames.
  bool TryToOutputFrames();

  SbDecodeTarget GetCurrentDecodeTarget() override;

  void UpdateDecodeTarget_Locked(const scoped_refptr<CpuVideoFrame>& frame);

  const bool may_reduce_quality_for_speed_;

  // The following callbacks will be initialized in Initialize() and won't be
  // changed during the life time of this class.
  DecoderStatusCB decoder_status_cb_;
  ErrorCB error_cb_;

  Size current_frame_size_;
  int frames_being_decoded_ = 0;
  Dav1dContext* dav1d_context_ = NULL;

  bool stream_ended_ = false;

  // Working thread to avoid lengthy decoding work block the player thread.
  std::unique_ptr<JobThread> decoder_thread_;

  // Decode-to-texture related state.
  const SbPlayerOutputMode output_mode_;

  SbDecodeTargetGraphicsContextProvider*
      decode_target_graphics_context_provider_;

  // If decode-to-texture is enabled, then we store the decode target texture
  // inside of this |decode_target_| member.
  SbDecodeTarget decode_target_;

  // GetCurrentDecodeTarget() needs to be called from an arbitrary thread
  // to obtain the current decode target (which ultimately ends up being a
  // copy of |decode_target_|), we need to safe-guard access to |decode_target_|
  // and we do so through this mutex.
  std::mutex decode_target_mutex_;

  std::queue<scoped_refptr<CpuVideoFrame>> frames_;
};

}  // namespace starboard

#endif  // STARBOARD_SHARED_LIBDAV1D_DAV1D_VIDEO_DECODER_H_
