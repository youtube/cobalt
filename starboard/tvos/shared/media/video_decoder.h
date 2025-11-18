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

#ifndef STARBOARD_TVOS_SHARED_MEDIA_VIDEO_DECODER_H_
#define STARBOARD_TVOS_SHARED_MEDIA_VIDEO_DECODER_H_

#import <VideoToolbox/VideoToolbox.h>

#include <atomic>
#include <limits>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

#include "starboard/common/ref_counted.h"
#include "starboard/shared/starboard/media/avc_util.h"
#include "starboard/shared/starboard/media/codec_util.h"
#include "starboard/shared/starboard/player/filter/video_decoder_internal.h"
#include "starboard/shared/starboard/player/job_queue.h"
#include "starboard/shared/starboard/player/job_thread.h"
#include "starboard/shared/starboard/thread_checker.h"

namespace starboard {

class TvosVideoDecoder : public VideoDecoder, private JobQueue::JobOwner {
 public:
  TvosVideoDecoder(SbPlayerOutputMode output_mode,
                   SbDecodeTargetGraphicsContextProvider*
                       decode_target_graphics_context_provider);
  ~TvosVideoDecoder() override;

  void Initialize(const DecoderStatusCB& decoder_status_cb,
                  const ErrorCB& error_cb) override;
  size_t GetPrerollFrameCount() const override {
    return GetMaxNumberOfCachedFrames();
  }
  int64_t GetPrerollTimeout() const override {
    return std::numeric_limits<int64_t>::max();
  }
  size_t GetMaxNumberOfCachedFrames() const override { return 8; }

  // VideoDecoder methods
  void WriteInputBuffers(const InputBuffers& input_buffers) override;
  void WriteEndOfStream() override;
  void Reset() override;
  SbDecodeTarget GetCurrentDecodeTarget() override;

 private:
  class DecodedImage : public RefCountedThreadSafe<DecodedImage> {
   public:
    DecodedImage(int64_t timestamp, CVImageBufferRef image_buffer)
        : timestamp_(timestamp), image_buffer_(image_buffer) {
      CFRetain(image_buffer);
    }
    ~DecodedImage() { CFRelease(image_buffer_); }

    int64_t timestamp() const { return timestamp_; }
    CVImageBufferRef image_buffer() const { return image_buffer_; }

   private:
    int64_t timestamp_;
    CVImageBufferRef image_buffer_;
  };

  static const int32_t kMaxDecodingFrames = 32;

  void WriteInputBufferInternal(const scoped_refptr<InputBuffer>& input_buffer);
  void WriteEndOfStreamInternal();

  OSStatus RefreshFormatAndSession(const AvcParameterSets& parameter_sets);
  void DestroyFormatAndSession();
  void OnCompletion(int64_t presentation_time, CVImageBufferRef image_buffer);
  void DestroyFrame(DecodedImage* decoded_image);

  void ReportOSError(const char* action, OSStatus status);
  void ReportGeneralError(const char* message);

  starboard::ThreadChecker thread_checker_;

  const int32_t application_inactive_counter_;

  SbDecodeTargetGraphicsContextProvider*
      decode_target_graphics_context_provider_;
  DecoderStatusCB decoder_status_cb_ = nullptr;
  ErrorCB error_cb_ = nullptr;

  std::unique_ptr<JobThread> job_thread_;

  CVOpenGLESTextureCacheRef texture_cache_ = nullptr;
  CMFormatDescriptionRef format_description_ = nullptr;
  VTDecompressionSessionRef session_ = nullptr;

  bool stream_ended_ = false;
  bool error_occurred_ = false;
  std::atomic_int32_t decoding_frames_{0};
  std::optional<VideoConfig> video_config_;
  uint64_t frame_counter_ = 0;
  scoped_refptr<VideoFrame> last_frame_;
  scoped_refptr<DecodedImage> last_decoded_image_;

  std::mutex decoded_images_mutex_;
  SbDecodeTarget current_decode_target_ = kSbDecodeTargetInvalid;
  std::list<scoped_refptr<DecodedImage>>
      decoded_images_;  // Guarded by |decoded_images_mutex_|.
  // Holding onto |kDecodeTargetReleaseQueueDepth| of decode targets before
  // release them just in case they are still being processed by the graphics
  // runtime.  Note that every decode target at 1080p holds ~3MB of memory,
  // and it is acceptable on Apple TV to hold a few of them.
  std::vector<SbDecodeTarget> decode_targets_to_release_;
};

}  // namespace starboard

#endif  // STARBOARD_TVOS_SHARED_MEDIA_VIDEO_DECODER_H_
