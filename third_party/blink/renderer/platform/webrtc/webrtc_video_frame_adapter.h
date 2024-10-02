// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WEBRTC_WEBRTC_VIDEO_FRAME_ADAPTER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WEBRTC_WEBRTC_VIDEO_FRAME_ADAPTER_H_

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_pool.h"
#include "media/base/video_types.h"
#include "media/capture/video/video_capture_feedback.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "media/video/renderable_gpu_memory_buffer_video_frame_pool.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/webrtc/api/scoped_refptr.h"
#include "third_party/webrtc/api/video/video_frame_buffer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

// The WebRtcVideoFrameAdapter implements webrtc::VideoFrameBuffer and is backed
// by one or more media::VideoFrames.
// * Upon CropAndScale(), the crop and scale values are soft-applied.
// * Upon GetMappedFrameBuffer(), any outstanding crop and scale is hard-applied
//   before returning the resulting buffer. This also happens on ToI420().
//
// This eliminates any intermediary downscales by only hard-applying what
// actually needs to be mapped.
//
// When crop and scale is hard-applied, the media::VideoFrame of closest size
// that is greater than or equal the requested size is chosen, minimizing
// scaling costs. If a media::VideoFrame exists in the desired size, no scaling
// is needed.
//
// WebRtcVideoFrameAdapter keeps track of which crops and scales were
// hard-applied during its lifetime.
// TODO(https://crbug.com/webrtc/12469): Expose this information to the caller
// or to the frame feeddback so that we may optionally use this information to
// optimize future captured frames for these sizes.
class PLATFORM_EXPORT WebRtcVideoFrameAdapter
    : public webrtc::VideoFrameBuffer {
 public:
  class VectorBufferPool {
   public:
    VectorBufferPool();
    ~VectorBufferPool() = default;
    // Allocate will return any available buffer and the vector buffer size
    // needs to be resized manually by the user.
    std::unique_ptr<std::vector<uint8_t>> Allocate();
    void Return(std::unique_ptr<std::vector<uint8_t>> buffer);

   private:
    struct BufferEntry {
      base::TimeTicks last_use_time;
      std::unique_ptr<std::vector<uint8_t>> buffer;
    };
    base::Lock buffer_lock_;
    Vector<BufferEntry> free_buffers_ GUARDED_BY(buffer_lock_);
    const base::TickClock* tick_clock_;
  };

  class PLATFORM_EXPORT SharedResources
      : public base::RefCountedThreadSafe<SharedResources> {
   public:
    explicit SharedResources(
        media::GpuVideoAcceleratorFactories* gpu_factories);

    // Create frames for requested output format and resolution.
    virtual scoped_refptr<media::VideoFrame> CreateFrame(
        media::VideoPixelFormat format,
        const gfx::Size& coded_size,
        const gfx::Rect& visible_rect,
        const gfx::Size& natural_size,
        base::TimeDelta timestamp);

    // Temporary vector buffers used in the video pre-processing for the input
    // frame before encoding, e.g. scaling the input frame to natural size for
    // encoding. Buffer needs manually release after using.
    virtual std::unique_ptr<std::vector<uint8_t>> CreateTemporaryVectorBuffer();
    virtual void ReleaseTemporaryVectorBuffer(
        std::unique_ptr<std::vector<uint8_t>> buffer);

    virtual scoped_refptr<viz::RasterContextProvider>
    GetRasterContextProvider();

    // Constructs a VideoFrame from a texture by invoking RasterInterface,
    // which would perform a blocking call to a GPU process.
    // The pixel data is copied and may be in ARGB pixel format in some cases,
    // So additional conversion to I420 would be needed.
    virtual scoped_refptr<media::VideoFrame> ConstructVideoFrameFromTexture(
        scoped_refptr<media::VideoFrame> source_frame);

    // Constructs a VideoFrame from a GMB. Unless it's a Windows DXGI GMB,
    // the buffer is mapped to the memory and wrapped by a VideoFrame with no
    // copies. For DXGI buffers a blocking call to GPU process is made to
    // copy pixel data.
    virtual scoped_refptr<media::VideoFrame> ConstructVideoFrameFromGpu(
        scoped_refptr<media::VideoFrame> source_frame);

    // Used to report feedback from an adapter upon destruction.
    void SetFeedback(const media::VideoCaptureFeedback& feedback);

    // Returns the most recently stored feedback.
    media::VideoCaptureFeedback GetFeedback();

   protected:
    friend class base::RefCountedThreadSafe<SharedResources>;
    virtual ~SharedResources();

   private:
    media::VideoFramePool pool_;
    media::VideoFramePool pool_for_mapped_frames_;
    VectorBufferPool pool_for_tmp_vectors_;

    std::unique_ptr<media::RenderableGpuMemoryBufferVideoFramePool>
        accelerated_frame_pool_;
    bool disable_gmb_frames_ = false;

    base::Lock context_provider_lock_;
    scoped_refptr<viz::RasterContextProvider> raster_context_provider_
        GUARDED_BY(context_provider_lock_);

    media::GpuVideoAcceleratorFactories* gpu_factories_;

    base::Lock feedback_lock_;

    // Contains feedback from the most recently destroyed Adapter.
    media::VideoCaptureFeedback last_feedback_ GUARDED_BY(feedback_lock_);
  };

  struct PLATFORM_EXPORT ScaledBufferSize {
    ScaledBufferSize(gfx::Rect visible_rect, gfx::Size natural_size);

    bool operator==(const ScaledBufferSize& rhs) const;
    bool operator!=(const ScaledBufferSize& rhs) const;

    // Applies crop-and-scale relative to the current natural size.
    ScaledBufferSize CropAndScale(int offset_x,
                                  int offset_y,
                                  int crop_width,
                                  int crop_height,
                                  int scaled_width,
                                  int scaled_height) const;

    // Cropping relative to the original image.
    gfx::Rect visible_rect;
    // The size (after scaling) of the visible rect.
    gfx::Size natural_size;
  };

  // Implements a soft-applied "view" of the parent WebRtcVideoFrameAdapter. Its
  // size only gets hard-applied if GetMappedFrameBuffer() or ToI420() is
  // called, in which case the result is cached inside the parent.
  class ScaledBuffer : public webrtc::VideoFrameBuffer {
   public:
    ScaledBuffer(scoped_refptr<WebRtcVideoFrameAdapter> parent,
                 ScaledBufferSize size);

    // Regardless of the pixel format used internally, kNative is returned
    // indicating that GetMappedFrameBuffer() or ToI420() is required to obtain
    // the pixels.
    webrtc::VideoFrameBuffer::Type type() const override {
      return webrtc::VideoFrameBuffer::Type::kNative;
    }
    int width() const override { return size_.natural_size.width(); }
    int height() const override { return size_.natural_size.height(); }

    // Obtains a mapped I420 buffer with this ScaledBuffer's size hard-applied.
    // If I420 is not used internally, a conversion happens.
    rtc::scoped_refptr<webrtc::I420BufferInterface> ToI420() override;

    // Obtains a mapped buffer of this ScaledBuffer's size hard-applied. The
    // resulting buffer's type is the non-kNative type used internally.
    rtc::scoped_refptr<webrtc::VideoFrameBuffer> GetMappedFrameBuffer(
        rtc::ArrayView<webrtc::VideoFrameBuffer::Type> types) override;

    // Soft-applies cropping and scaling. The result is another ScaledBuffer.
    rtc::scoped_refptr<webrtc::VideoFrameBuffer> CropAndScale(
        int offset_x,
        int offset_y,
        int crop_width,
        int crop_height,
        int scaled_width,
        int scaled_height) override;

    const ScaledBufferSize& size() const { return size_; }

   private:
    scoped_refptr<WebRtcVideoFrameAdapter> parent_;
    ScaledBufferSize size_;
  };

  explicit WebRtcVideoFrameAdapter(scoped_refptr<media::VideoFrame> frame);
  WebRtcVideoFrameAdapter(
      scoped_refptr<media::VideoFrame> frame,
      std::vector<scoped_refptr<media::VideoFrame>> scaled_frames,
      scoped_refptr<SharedResources> shared_resources);

  scoped_refptr<media::VideoFrame> getMediaVideoFrame() const { return frame_; }

  // Regardless of the pixel format used internally, kNative is returned
  // indicating that GetMappedFrameBuffer() or ToI420() is required to obtain
  // the pixels.
  webrtc::VideoFrameBuffer::Type type() const override {
    return webrtc::VideoFrameBuffer::Type::kNative;
  }
  int width() const override { return frame_->natural_size().width(); }
  int height() const override { return frame_->natural_size().height(); }

  rtc::scoped_refptr<webrtc::I420BufferInterface> ToI420() override;
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> GetMappedFrameBuffer(
      rtc::ArrayView<webrtc::VideoFrameBuffer::Type> types) override;

  // Soft-applies cropping and scaling. The result is a ScaledBuffer.
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> CropAndScale(
      int offset_x,
      int offset_y,
      int crop_width,
      int crop_height,
      int scaled_width,
      int scaled_height) override;

  // If this exact size has been hard-applied by wrapping or using a pre-scaled
  // media::VideoFrame, the associated media::VideoFrame is returned (the
  // wrapping or original). If this size has not been hard-applied, or it was
  // hard-applied by scaling a previously adapted webrtc::VideoFrameBuffer, then
  // null is returned.
  scoped_refptr<media::VideoFrame> GetAdaptedVideoBufferForTesting(
      const ScaledBufferSize& size);

 protected:
  ~WebRtcVideoFrameAdapter() override;

 private:
  struct AdaptedFrame {
    AdaptedFrame(ScaledBufferSize size,
                 scoped_refptr<media::VideoFrame> video_frame,
                 rtc::scoped_refptr<webrtc::VideoFrameBuffer> frame_buffer)
        : size(std::move(size)),
          video_frame(std::move(video_frame)),
          frame_buffer(std::move(frame_buffer)) {}

    ScaledBufferSize size;
    // If |frame_buffer| was produced without a media::VideoFrame this is null.
    scoped_refptr<media::VideoFrame> video_frame;
    rtc::scoped_refptr<webrtc::VideoFrameBuffer> frame_buffer;
  };

  rtc::scoped_refptr<webrtc::VideoFrameBuffer> GetOrCreateFrameBufferForSize(
      const ScaledBufferSize& size);
  AdaptedFrame AdaptBestFrame(const ScaledBufferSize& size) const
      EXCLUSIVE_LOCKS_REQUIRED(adapted_frames_lock_);

  base::Lock adapted_frames_lock_;
  const scoped_refptr<media::VideoFrame> frame_;
  const Vector<scoped_refptr<media::VideoFrame>> scaled_frames_;
  const scoped_refptr<SharedResources> shared_resources_;
  const ScaledBufferSize full_size_;
  // Frames that have been adapted, i.e. that were "hard-applied" and mapped.
  Vector<AdaptedFrame> adapted_frames_ GUARDED_BY(adapted_frames_lock_);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WEBRTC_WEBRTC_VIDEO_FRAME_ADAPTER_H_
