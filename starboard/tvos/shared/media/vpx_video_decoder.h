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

#ifndef STARBOARD_TVOS_SHARED_MEDIA_VPX_VIDEO_DECODER_H_
#define STARBOARD_TVOS_SHARED_MEDIA_VPX_VIDEO_DECODER_H_

#import <OpenGLES/EAGL.h>
#include <vpx/vp8dx.h>
#include <vpx/vpx_decoder.h>

#include <list>
#include <queue>
#include <string>

#include "starboard/common/log.h"
#include "starboard/common/ref_counted.h"
#include "starboard/media.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/filter/video_decoder_internal.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/shared/starboard/player/job_thread.h"
#include "starboard/shared/starboard/thread_checker.h"
#include "starboard/tvos/shared/media/decode_target_internal.h"
#include "starboard/tvos/shared/media/vpx_loop_filter_manager.h"

namespace starboard {
namespace shared {
namespace uikit {

class VpxVideoDecoder : public starboard::player::filter::VideoDecoder {
 public:
  class TextureProvider;
  class DecodedImageData;
  class DecodeTargetData;

  VpxVideoDecoder(SbPlayerOutputMode output_mode,
                  SbDecodeTargetGraphicsContextProvider*
                      decode_target_graphics_context_provider);
  ~VpxVideoDecoder() override;

  static bool Is4KSupported();

  void Initialize(const DecoderStatusCB& decoder_status_cb,
                  const ErrorCB& error_cb) override;
  size_t GetPrerollFrameCount() const override;
  int64_t GetPrerollTimeout() const override { return kSbInt64Max; }
  size_t GetMaxNumberOfCachedFrames() const override {
    return kMaxNumberOfCachedFrames;
  }

  void WriteInputBuffers(const InputBuffers& input_buffers) override;
  void WriteEndOfStream() override;
  void Reset() override;

 private:
  static const size_t kMinimumPrerollFrameCount = 8;
  static const size_t kMaxSizeOfDecodedImages = 16;
  static const size_t kMaxNumberOfCachedFrames = 64;
  // Loop filter is disabled when the frame backlog is below this.
  static const size_t kDisableLoopFilterLowWatermark = 32;
  // Loop filter is re-enabled when the frame backlog goes beyond this after
  // loop filter is disabled.
  static const size_t kDisableLoopFilterHighWatermark = 60;
  // No longer ask for more input buffer if the number of cached input buffers
  // exceeds the following constant.
  static const size_t kMaxNumberOfCachedInputBuffer = 32;

  void ReportError(const std::string& error_message);

  // The following six functions are only called on the decoder thread.
  void InitializeCodec();
  void TeardownCodec(bool clear_all_decoded_frames);
  void DecodeOneBuffer();
  void DecodeEndOfStream();
  void FlushPendingBuffers();
  // Return true if get a new output frame, otherwise, return false.
  bool TryGetOneFrame();

  // The following four functions are only called on the texture thread.
  void InitializeTextureThread();
  void TeardownTextureThread(bool clear_all_decoded_frames);
  void GenerateDecodeTargetData();
  void GenerateEndOfStream();

  // The following two function are called on other threads.
  void RemoveDecodedImageByTimestamp(int64_t timestamp);
  SbDecodeTarget GetCurrentDecodeTarget() override;

  // The following callbacks will be initialized in Initialize() and won't be
  // changed during the life time of this class.
  DecoderStatusCB decoder_status_cb_;
  ErrorCB error_cb_;

  int current_frame_width_ = 0;
  int current_frame_height_ = 0;
  int total_input_frames_ = 0;
  int total_output_frames_ = 0;
  std::unique_ptr<vpx_codec_ctx> context_;

  bool stream_ended_ = false;
  bool error_occurred_ = false;

  starboard::ThreadChecker thread_checker_;
  // Working thread to avoid lengthy decoding work block the player thread.
  starboard::player::ScopedJobThreadPtr decoder_thread_;
  starboard::player::ScopedJobThreadPtr texture_thread_;

  SbDecodeTargetGraphicsContextProvider*
      decode_target_graphics_context_provider_;
  EAGLContext* egl_context_ = nullptr;

  Mutex pending_buffers_mutex_;
  std::list<scoped_refptr<InputBuffer>> pending_buffers_;
  size_t pending_buffers_total_size_in_bytes_ = 0;

  Mutex decoded_images_mutex_;
  std::queue<std::unique_ptr<DecodedImageData>> decoded_images_;
  std::atomic_int32_t decoded_images_size_{0};

  Mutex decode_target_data_queue_mutex_;
  std::queue<scoped_refptr<DecodeTargetData>> decode_target_data_queue_;
  std::atomic_int32_t decode_target_data_queue_size_{0};

  scoped_refptr<TextureProvider> texture_provider_;
  scoped_refptr<DecodeTargetData> current_decode_target_data_;
  std::atomic_int64_t last_removed_timestamp_{-1};
  std::atomic_bool need_update_current_frame_{true};

  VpxLoopFilterManager loop_filter_manager_;
  int frames_with_loop_filter_disabled_ = 0;
  bool loop_filter_disabled_ = false;
};

}  // namespace uikit
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_TVOS_SHARED_MEDIA_VPX_VIDEO_DECODER_H_
