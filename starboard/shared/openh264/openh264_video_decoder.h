// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_OPENH264_OPENH264_VIDEO_DECODER_H_
#define STARBOARD_SHARED_OPENH264_OPENH264_VIDEO_DECODER_H_

#include <queue>
#include <string>
#include <vector>

#include "starboard/common/optional.h"
#include "starboard/common/ref_counted.h"
#include "starboard/decode_target.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/media/codec_util.h"
#include "starboard/shared/starboard/player/filter/cpu_video_frame.h"
#include "starboard/shared/starboard/player/filter/video_decoder_internal.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/shared/starboard/player/job_thread.h"
#include "third_party/openh264/include/codec_api.h"
#include "third_party/openh264/include/codec_app_def.h"
#include "third_party/openh264/include/codec_def.h"

namespace starboard {
namespace shared {
namespace openh264 {

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

  // TODO: Verify if these values are correct.
  size_t GetPrerollFrameCount() const override { return 8; }
  SbTime GetPrerollTimeout() const override { return kSbTimeMax; }
  size_t GetMaxNumberOfCachedFrames() const override { return 12; }

  void WriteInputBuffers(const InputBuffers& input_buffers) override;
  void WriteEndOfStream() override;
  void Reset() override;

  SbDecodeTarget GetCurrentDecodeTarget() override;

 private:
  static const int kDefaultOpenH264BitsDepth = 8;
  typedef ::starboard::shared::starboard::player::filter::CpuVideoFrame
      CpuVideoFrame;
  // Operator to compare CpuVideoFrame by timestamp.
  struct VideoFrameTimeStampGreater {
    bool operator()(const scoped_refptr<CpuVideoFrame>& left,
                    const scoped_refptr<CpuVideoFrame>& right) const {
      // In chronological order.
      return left->timestamp() > right->timestamp();
    }
  };
  typedef std::priority_queue<scoped_refptr<CpuVideoFrame>,
                              std::vector<scoped_refptr<CpuVideoFrame>>,
                              VideoFrameTimeStampGreater>
      TimeSequentialQueue;

  void UpdateDecodeTarget_Locked(const scoped_refptr<CpuVideoFrame>& frame);

  // The following functions are only called on the decoder thread.
  void InitializeCodec();
  void TeardownCodec();
  void DecodeOneBuffer(const scoped_refptr<InputBuffer>& input_buffer);
  void DecodeEndOfStream();
  void ProcessDecodedImage(unsigned char* decoded_frame[],
                           const SBufferInfo& buffer_info,
                           bool flushing);
  void FlushFrames();
  void ReportError(const std::string& error_message);

  SbDecodeTargetGraphicsContextProvider*
      decode_target_graphics_context_provider_;
  SbPlayerOutputMode output_mode_;

  // The following callbacks will be initialized in Initialize() and won't be
  // changed during the life time of this class.
  DecoderStatusCB decoder_status_cb_;
  ErrorCB error_cb_;

  // Openh264 does NOT always output video frames in chronological order.
  // |time_sequential_queue_| is used to reorder |CpuVideoFrame|
  // chronologically.
  TimeSequentialQueue time_sequential_queue_;

  std::queue<scoped_refptr<CpuVideoFrame>> frames_;

  bool stream_ended_ = false;

  // If decode-to-texture is enabled, then we store the decode target texture
  // inside of this |decode_target_| member.
  SbDecodeTarget decode_target_ = kSbDecodeTargetInvalid;

  // GetCurrentDecodeTarget() needs to be called from an arbitrary thread
  // to obtain the current decode target (which ultimately ends up being a
  // copy of |decode_target_|), we need to safe-guard access to |decode_target_|
  // and we do so through this mutex.
  Mutex decode_target_mutex_;

  // Working thread to avoid lengthy decoding work block the player thread.
  scoped_ptr<starboard::player::JobThread> decoder_thread_;

  // Openh264 decode handler.
  ISVCDecoder* decoder_ = nullptr;

  // The number of frames which have been sent to decoder but not received yet.
  int frames_being_decoded_ = 0;

  // Store current avc level profile and resolution.
  optional<shared::starboard::media::VideoConfig> video_config_;
};

}  // namespace openh264
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_OPENH264_OPENH264_VIDEO_DECODER_H_
