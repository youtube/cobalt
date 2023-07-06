// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_XB1_SHARED_GPU_BASE_VIDEO_DECODER_H_
#define STARBOARD_XB1_SHARED_GPU_BASE_VIDEO_DECODER_H_

#include <d3d11_1.h>
#include <d3d12.h>
#include <wrl/client.h>

#include <atomic>
#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "starboard/common/atomic.h"
#include "starboard/common/mutex.h"
#include "starboard/common/ref_counted.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/shared/starboard/decode_target/decode_target_context_runner.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/filter/video_decoder_internal.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/shared/starboard/player/job_thread.h"
#include "starboard/xb1/shared/video_frame_impl.h"

namespace starboard {
namespace xb1 {
namespace shared {

class GpuVideoDecoderBase
    : public starboard::shared::starboard::player::filter::VideoDecoder,
      private starboard::shared::starboard::player::JobQueue::JobOwner {
 public:
  static constexpr unsigned int kNumberOfPlanes = 3;

  typedef ::starboard::shared::starboard::decode_target::
      DecodeTargetContextRunner DecodeTargetContextRunner;

  ~GpuVideoDecoderBase() override;

 protected:
  typedef ::starboard::shared::starboard::media::VideoStreamInfo
      VideoStreamInfo;

  class DecodedImage : public RefCountedThreadSafe<DecodedImage> {
   public:
    virtual ~DecodedImage() {
      if (release_cb_) {
        release_cb_();
      }
    }

    bool is_compacted() const { return is_compacted_; }
    unsigned int width() const { return width_; }
    unsigned int height() const { return height_; }
    unsigned int bit_depth() const { return bit_depth_; }
    int texture_corner_left(size_t index) const {
      SB_DCHECK(index < kNumberOfPlanes);
      return texture_corner_left_[index];
    }
    int texture_corner_top(size_t index) const {
      SB_DCHECK(index < kNumberOfPlanes);
      return texture_corner_top_[index];
    }
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture(size_t index) const {
      SB_DCHECK(index < kNumberOfPlanes);
      return textures_[index];
    }
    int stride(size_t index) const {
      SB_DCHECK(index < kNumberOfPlanes);
      return strides_[index];
    }
    SbTime timestamp() const { return timestamp_; }
    const SbMediaColorMetadata& color_metadata() const {
      return color_metadata_;
    }

    void AttachColorMetadata(const SbMediaColorMetadata& color_metadata) {
      color_metadata_ = color_metadata;
    }

   protected:
    explicit DecodedImage(const std::function<void(void)>& release_cb = nullptr)
        : release_cb_(release_cb) {}

    unsigned int width_;
    unsigned int height_;
    unsigned int bit_depth_;
    bool is_compacted_ = false;
    int texture_corner_left_[kNumberOfPlanes];
    int texture_corner_top_[kNumberOfPlanes];
    Microsoft::WRL::ComPtr<ID3D11Texture2D> textures_[kNumberOfPlanes];
    int strides_[kNumberOfPlanes];
    SbTime timestamp_;
    SbMediaColorMetadata color_metadata_;
    const std::function<void(void)> release_cb_;
  };

  enum RetrievingBehavior {
    kDecodingStopped,
    kDecodingFrames,
    kResettingDecoder,
    kEndingStream
  };

  GpuVideoDecoderBase(SbDecodeTargetGraphicsContextProvider*
                          decode_target_graphics_context_provider,
                      const VideoStreamInfo& video_stream_info,
                      bool is_hdr_video,
                      const Microsoft::WRL::ComPtr<ID3D12Device>& d3d12_device,
                      void* d3d12_queue);

  // VideoDecoder methods
  void Initialize(const DecoderStatusCB& decoder_status_cb,
                  const ErrorCB& error_cb) final;
  SbTime GetPrerollTimeout() const final { return kSbTimeMax; }
  size_t GetMaxNumberOfCachedFrames() const override;

  void WriteInputBuffers(const InputBuffers& input_buffers) final;
  void WriteEndOfStream() final;
  void Reset() final;
  SbDecodeTarget GetCurrentDecodeTarget() final;

  // Methods for inherited classes to implement.
  virtual size_t GetMaxNumberOfCachedFramesInternal() const = 0;
  virtual void InitializeCodecIfNeededInternal() = 0;
  virtual void DecodeInternal(
      const scoped_refptr<InputBuffer>& input_buffer) = 0;
  virtual void DrainDecoderInternal() = 0;

  bool BelongsToDecoderThread() const;
  void OnOutputRetrieved(const scoped_refptr<DecodedImage>& image);
  void OnDecoderDrained();
  void ClearCachedImages();
  void ReportError(const SbPlayerError error, const std::string& error_message);

  const bool is_hdr_video_;
  int frame_width_;
  int frame_height_;
  atomic_integral<RetrievingBehavior> decoder_behavior_{kDecodingStopped};
  std::atomic_bool error_occured_ = {false};

  // These are platform-specific objects required to create and use a codec.
  Microsoft::WRL::ComPtr<ID3D11Device1> d3d11_device_;
  Microsoft::WRL::ComPtr<ID3D12Device> d3d12_device_;
  void* d3d12_queue_ = nullptr;

 private:
  class GPUDecodeTargetPrivate;

  bool IsCacheFull();
  void DecodeOneBuffer();
  void DecodeEndOfStream();
  void DrainDecoder();
  void DeleteVideoFrame(const VideoFrame* video_frame);
  void UpdateHdrMetadata(const SbMediaColorMetadata& color_metadata);
  void ClearPresentingDecodeTargets();

  // The following callbacks will be initialized in Initialize() and won't be
  // changed during the life time of this class.
  DecoderStatusCB decoder_status_cb_;
  ErrorCB error_cb_;

  void* egl_config_ = nullptr;
  void* egl_display_ = nullptr;
  DecodeTargetContextRunner decode_target_context_runner_;

  scoped_ptr<starboard::shared::starboard::player::JobThread> decoder_thread_;

  // |pending_inputs_| is shared between player main thread and decoder thread.
  Mutex pending_inputs_mutex_;
  std::deque<scoped_refptr<InputBuffer>> pending_inputs_;
  // |written_inputs_| is only accessed on decoder thread.
  std::vector<scoped_refptr<InputBuffer>> written_inputs_;
  // |output_queue_| is shared between decoder thread and render thread.
  Mutex output_queue_mutex_;
  std::vector<scoped_refptr<DecodedImage>> output_queue_;
  // |presenting_decode_targets_| is only accessed on render thread.
  std::deque<GPUDecodeTargetPrivate*> presenting_decode_targets_;
  std::atomic_int number_of_presenting_decode_targets_ = {0};

  SbMediaColorMetadata last_presented_color_metadata_ = {};

  bool is_drain_decoder_called_ = false;

  bool needs_hdr_metadata_update_ = true;
};

}  // namespace shared
}  // namespace xb1
}  // namespace starboard

#endif  // STARBOARD_XB1_SHARED_GPU_BASE_VIDEO_DECODER_H_
