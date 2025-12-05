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

#ifndef STARBOARD_TVOS_SHARED_MEDIA_VP9_SW_AV_VIDEO_SAMPLE_BUFFER_BUILDER_H_
#define STARBOARD_TVOS_SHARED_MEDIA_VP9_SW_AV_VIDEO_SAMPLE_BUFFER_BUILDER_H_

#include <vpx/vp8dx.h>
#include <vpx/vpx_decoder.h>

#include <atomic>
#include <map>
#include <mutex>
#include <queue>
#include <utility>

#include "starboard/common/ref_counted.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/job_thread.h"
#import "starboard/tvos/shared/media/av_video_sample_buffer_builder.h"

// #define VP9_SW_DECODER_PRINT_OUT_DECODING_TIME 1

namespace starboard {

class Vp9SwAVVideoSampleBufferBuilder : public AVVideoSampleBufferBuilder {
 public:
  explicit Vp9SwAVVideoSampleBufferBuilder(
      const VideoStreamInfo& video_stream_info);
  ~Vp9SwAVVideoSampleBufferBuilder() override;
  void Reset() override;
  void WriteInputBuffer(const scoped_refptr<InputBuffer>& input_buffer,
                        int64_t media_time_offset) override;
  void WriteEndOfStream() override;
  size_t GetPrerollFrameCount() const override;
  bool IsSampleAlreadyDecoded() const override { return true; }
  int64_t DecodingTimeNeededPerFrame() const override { return 0; }
  size_t GetMaxNumberOfCachedFrames() const override;
  void OnCachedFramesWatermarkLow() override;
  void OnCachedFramesWatermarkHigh() override;

 private:
  class VpxImageWrapper {
   public:
    VpxImageWrapper(const scoped_refptr<InputBuffer>& input_buffer,
                    const vpx_image_t& vpx_image);
    ~VpxImageWrapper();
    const scoped_refptr<InputBuffer>& input_buffer() const {
      return input_buffer_;
    }
    const vpx_image_t& vpx_image() const { return vpx_image_; }

   private:
    const scoped_refptr<InputBuffer> input_buffer_;
    const vpx_image_t vpx_image_;
  };

  // The following six functions are only called on the |decoder_thread_|.
  void InitializeCodec();
  void TeardownCodec();
  void DecodeOneBuffer();
  void DecodeEndOfStream();
  void FlushPendingBuffers();
  // Return true if get a new output frame, otherwise, return false.
  bool TryGetOneFrame();

  // BuildSampleBuffer() is only called on the |builder_thread_|.
  void BuildSampleBuffer();

  void CreatePixelBufferPool();

  const bool is_hdr_;

  std::unique_ptr<vpx_codec_ctx> context_;
  unsigned int current_frame_width_ = 0;
  unsigned int current_frame_height_ = 0;
  int total_input_frames_ = 0;
  int total_output_frames_ = 0;
  std::atomic_uint64_t preroll_frame_count_ = {0};
  std::atomic_uint64_t max_number_of_cached_buffers_ = {0};
  bool stream_ended_ = false;

  std::atomic_bool skip_loop_filter_ = {false};
  size_t frames_with_skip_loop_filter_ = 0;
  bool skip_loop_filter_on_decoder_thread_ = false;

  std::unique_ptr<JobThread> decoder_thread_;
  std::unique_ptr<JobThread> builder_thread_;

  std::mutex pending_input_buffers_mutex_;
  std::queue<scoped_refptr<InputBuffer>>
      pending_input_buffers_;  // Guarded by |pending_input_buffers_mutex_|.
  int64_t media_time_offset_ = 0;

  // |decoding_input_buffers_| is only used on decoder thread.
  std::map<int64_t, const scoped_refptr<InputBuffer>> decoding_input_buffers_;

  std::mutex decoded_images_mutex_;
  std::queue<std::unique_ptr<VpxImageWrapper>>
      decoded_images_;  // Guarded by |decoded_images_mutex_|.

  CVPixelBufferPoolRef pixel_buffer_pool_ = nullptr;
  CFDictionaryRef pixel_buffer_attachments_ = nullptr;

#ifdef VP9_SW_DECODER_PRINT_OUT_DECODING_TIME
  std::queue<std::tuple<int64_t, int64_t, size_t>> decoding_times_;
  int64_t total_decoding_time_in_last_second_ = 0;
  size_t total_bits_in_last_second_ = 0;
#endif  // VP9_SW_DECODER_PRINT_OUT_DECODING_TIME
};

}  // namespace starboard

#endif  // STARBOARD_TVOS_SHARED_MEDIA_VP9_SW_AV_VIDEO_SAMPLE_BUFFER_BUILDER_H_
