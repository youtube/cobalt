// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"

#include <limits>
#include <utility>

#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/checked_math.h"
#include "base/task/bind_post_task.h"
#include "base/time/time.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "media/base/limits.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_metadata.h"
#include "media/base/video_frame_pool.h"
#include "media/base/video_types.h"
#include "media/base/video_util.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-blink.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_plane_layout.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_cssimagevalue_htmlcanvaselement_htmlimageelement_htmlvideoelement_imagebitmap_offscreencanvas_svgimageelement_videoframe.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_color_space_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_frame_buffer_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_frame_copy_to_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_frame_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_pixel_format.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_state_observer.h"
#include "third_party/blink/renderer/core/frame/deprecation/deprecation.h"
#include "third_party/blink/renderer/core/geometry/dom_rect_read_only.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_image_source.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/canvas/imagebitmap/image_bitmap_factories.h"
#include "third_party/blink/renderer/modules/webcodecs/background_readback.h"
#include "third_party/blink/renderer/modules/webcodecs/video_color_space.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame_init_util.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame_layout.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame_rect_util.h"
#include "third_party/blink/renderer/platform/geometry/geometry_hash_traits.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/graphics/skia/sk_image_info_hash.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/video_frame_image_util.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_handle.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/libyuv/include/libyuv/planar_functions.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "v8/include/v8.h"

namespace WTF {

template <>
struct CrossThreadCopier<blink::VideoFrameLayout>
    : public CrossThreadCopierPassThrough<blink::VideoFrameLayout> {
  STATIC_ONLY(CrossThreadCopier);
};

}  // namespace WTF

namespace blink {

namespace {

media::VideoPixelFormat ToMediaPixelFormat(V8VideoPixelFormat::Enum fmt) {
  switch (fmt) {
    case V8VideoPixelFormat::Enum::kI420:
      return media::PIXEL_FORMAT_I420;
    case V8VideoPixelFormat::Enum::kI420A:
      return media::PIXEL_FORMAT_I420A;
    case V8VideoPixelFormat::Enum::kI422:
      return media::PIXEL_FORMAT_I422;
    case V8VideoPixelFormat::Enum::kI444:
      return media::PIXEL_FORMAT_I444;
    case V8VideoPixelFormat::Enum::kNV12:
      return media::PIXEL_FORMAT_NV12;
    case V8VideoPixelFormat::Enum::kRGBA:
      return media::PIXEL_FORMAT_ABGR;
    case V8VideoPixelFormat::Enum::kRGBX:
      return media::PIXEL_FORMAT_XBGR;
    case V8VideoPixelFormat::Enum::kBGRA:
      return media::PIXEL_FORMAT_ARGB;
    case V8VideoPixelFormat::Enum::kBGRX:
      return media::PIXEL_FORMAT_XRGB;
  }
}

media::VideoPixelFormat ToOpaqueMediaPixelFormat(media::VideoPixelFormat fmt) {
  DCHECK(!media::IsOpaque(fmt));
  switch (fmt) {
    case media::PIXEL_FORMAT_I420A:
      return media::PIXEL_FORMAT_I420;
    case media::PIXEL_FORMAT_ARGB:
      return media::PIXEL_FORMAT_XRGB;
    case media::PIXEL_FORMAT_ABGR:
      return media::PIXEL_FORMAT_XBGR;
    default:
      NOTIMPLEMENTED() << "Missing support for making " << fmt << " opaque.";
      return fmt;
  }
}

absl::optional<V8VideoPixelFormat> ToV8VideoPixelFormat(
    media::VideoPixelFormat fmt) {
  switch (fmt) {
    case media::PIXEL_FORMAT_I420:
      return V8VideoPixelFormat(V8VideoPixelFormat::Enum::kI420);
    case media::PIXEL_FORMAT_I420A:
      return V8VideoPixelFormat(V8VideoPixelFormat::Enum::kI420A);
    case media::PIXEL_FORMAT_I422:
      return V8VideoPixelFormat(V8VideoPixelFormat::Enum::kI422);
    case media::PIXEL_FORMAT_I444:
      return V8VideoPixelFormat(V8VideoPixelFormat::Enum::kI444);
    case media::PIXEL_FORMAT_NV12:
      return V8VideoPixelFormat(V8VideoPixelFormat::Enum::kNV12);
    case media::PIXEL_FORMAT_ABGR:
      return V8VideoPixelFormat(V8VideoPixelFormat::Enum::kRGBA);
    case media::PIXEL_FORMAT_XBGR:
      return V8VideoPixelFormat(V8VideoPixelFormat::Enum::kRGBX);
    case media::PIXEL_FORMAT_ARGB:
      return V8VideoPixelFormat(V8VideoPixelFormat::Enum::kBGRA);
    case media::PIXEL_FORMAT_XRGB:
      return V8VideoPixelFormat(V8VideoPixelFormat::Enum::kBGRX);
    default:
      NOTREACHED();
      return absl::nullopt;
  }
}

class CachedVideoFramePool : public GarbageCollected<CachedVideoFramePool>,
                             public Supplement<ExecutionContext>,
                             public ExecutionContextLifecycleStateObserver {
 public:
  static const char kSupplementName[];

  static CachedVideoFramePool& From(ExecutionContext& context) {
    CachedVideoFramePool* supplement =
        Supplement<ExecutionContext>::From<CachedVideoFramePool>(context);
    if (!supplement) {
      supplement = MakeGarbageCollected<CachedVideoFramePool>(context);
      Supplement<ExecutionContext>::ProvideTo(context, supplement);
    }
    return *supplement;
  }

  explicit CachedVideoFramePool(ExecutionContext& context)
      : Supplement<ExecutionContext>(context),
        ExecutionContextLifecycleStateObserver(&context) {
    UpdateStateIfNeeded();
  }
  ~CachedVideoFramePool() override = default;

  // Disallow copy and assign.
  CachedVideoFramePool& operator=(const CachedVideoFramePool&) = delete;
  CachedVideoFramePool(const CachedVideoFramePool&) = delete;

  scoped_refptr<media::VideoFrame> CreateFrame(media::VideoPixelFormat format,
                                               const gfx::Size& coded_size,
                                               const gfx::Rect& visible_rect,
                                               const gfx::Size& natural_size,
                                               base::TimeDelta timestamp) {
    if (!frame_pool_)
      CreatePoolAndStartIdleObsever();

    last_frame_creation_ = base::TimeTicks::Now();
    return frame_pool_->CreateFrame(format, coded_size, visible_rect,
                                    natural_size, timestamp);
  }

  void Trace(Visitor* visitor) const override {
    Supplement<ExecutionContext>::Trace(visitor);
    ExecutionContextLifecycleStateObserver::Trace(visitor);
  }

  void ContextLifecycleStateChanged(
      mojom::blink::FrameLifecycleState state) override {
    if (state == mojom::blink::FrameLifecycleState::kRunning)
      return;
    // Reset `frame_pool_` because the task runner for purging will get paused.
    frame_pool_.reset();
    task_handle_.Cancel();
  }

  void ContextDestroyed() override { frame_pool_.reset(); }

 private:
  static const base::TimeDelta kIdleTimeout;

  void PostMonitoringTask() {
    DCHECK(!task_handle_.IsActive());
    task_handle_ = PostDelayedCancellableTask(
        *GetSupplementable()->GetTaskRunner(TaskType::kInternalMedia),
        FROM_HERE,
        WTF::BindOnce(&CachedVideoFramePool::PurgeIdleFramePool,
                      WrapWeakPersistent(this)),
        kIdleTimeout);
  }

  void CreatePoolAndStartIdleObsever() {
    DCHECK(!frame_pool_);
    frame_pool_ = std::make_unique<media::VideoFramePool>();
    PostMonitoringTask();
  }

  // We don't want a VideoFramePool to stick around forever wasting memory, so
  // once we haven't issued any VideoFrames for a while, turn down the pool.
  void PurgeIdleFramePool() {
    if (base::TimeTicks::Now() - last_frame_creation_ > kIdleTimeout) {
      frame_pool_.reset();
      return;
    }
    PostMonitoringTask();
  }

  std::unique_ptr<media::VideoFramePool> frame_pool_;
  base::TimeTicks last_frame_creation_;
  TaskHandle task_handle_;
};

// static -- defined out of line to satisfy link time requirements.
const char CachedVideoFramePool::kSupplementName[] = "CachedVideoFramePool";
const base::TimeDelta CachedVideoFramePool::kIdleTimeout = base::Seconds(10);

class CanvasResourceProviderCache
    : public GarbageCollected<CanvasResourceProviderCache>,
      public Supplement<ExecutionContext>,
      public ExecutionContextLifecycleStateObserver {
 public:
  static const char kSupplementName[];

  static CanvasResourceProviderCache& From(ExecutionContext& context) {
    CanvasResourceProviderCache* supplement =
        Supplement<ExecutionContext>::From<CanvasResourceProviderCache>(
            context);
    if (!supplement) {
      supplement = MakeGarbageCollected<CanvasResourceProviderCache>(context);
      Supplement<ExecutionContext>::ProvideTo(context, supplement);
    }
    return *supplement;
  }

  explicit CanvasResourceProviderCache(ExecutionContext& context)
      : Supplement<ExecutionContext>(context),
        ExecutionContextLifecycleStateObserver(&context) {
    UpdateStateIfNeeded();
  }
  ~CanvasResourceProviderCache() override = default;

  // Disallow copy and assign.
  CanvasResourceProviderCache& operator=(const CanvasResourceProviderCache&) =
      delete;
  CanvasResourceProviderCache(const CanvasResourceProviderCache&) = delete;

  CanvasResourceProvider* CreateProvider(const SkImageInfo& info) {
    if (info_to_provider_.empty())
      PostMonitoringTask();

    last_access_time_ = base::TimeTicks::Now();

    auto iter = info_to_provider_.find(info);
    if (iter != info_to_provider_.end()) {
      auto* result = iter->value.get();
      if (result && result->IsValid())
        return result;
    }

    if (info_to_provider_.size() >= kMaxSize)
      info_to_provider_.clear();

    auto provider = CreateResourceProviderForVideoFrame(
        info, GetRasterContextProvider().get());
    auto* result = provider.get();
    info_to_provider_.Set(info, std::move(provider));
    return result;
  }

  void Trace(Visitor* visitor) const override {
    Supplement<ExecutionContext>::Trace(visitor);
    ExecutionContextLifecycleStateObserver::Trace(visitor);
  }

  void ContextLifecycleStateChanged(
      mojom::blink::FrameLifecycleState state) override {
    if (state == mojom::blink::FrameLifecycleState::kRunning)
      return;
    // Reset `info_to_provider_` because the task runner for purging will get
    // paused.
    info_to_provider_.clear();
    task_handle_.Cancel();
  }

  void ContextDestroyed() override { info_to_provider_.clear(); }

 private:
  static constexpr int kMaxSize = 50;
  static const base::TimeDelta kIdleTimeout;

  void PostMonitoringTask() {
    DCHECK(!task_handle_.IsActive());
    task_handle_ = PostDelayedCancellableTask(
        *GetSupplementable()->GetTaskRunner(TaskType::kInternalMedia),
        FROM_HERE,
        WTF::BindOnce(&CanvasResourceProviderCache::PurgeIdleFramePool,
                      WrapWeakPersistent(this)),
        kIdleTimeout);
  }

  void PurgeIdleFramePool() {
    if (base::TimeTicks::Now() - last_access_time_ > kIdleTimeout) {
      info_to_provider_.clear();
      return;
    }
    PostMonitoringTask();
  }

  HashMap<SkImageInfo, std::unique_ptr<CanvasResourceProvider>>
      info_to_provider_;
  base::TimeTicks last_access_time_;
  TaskHandle task_handle_;
};

// static -- defined out of line to satisfy link time requirements.
const char CanvasResourceProviderCache::kSupplementName[] =
    "CanvasResourceProviderCache";
const base::TimeDelta CanvasResourceProviderCache::kIdleTimeout =
    base::Seconds(10);

absl::optional<media::VideoPixelFormat> CopyToFormat(
    const media::VideoFrame& frame) {
  const bool mappable = frame.IsMappable() || frame.HasGpuMemoryBuffer();
  const bool texturable = frame.HasTextures();
  if (!(mappable || texturable))
    return absl::nullopt;

  // The |frame|.BitDepth() restriction is to avoid treating a P016LE frame as a
  // low-bit depth frame.
  if (!mappable && frame.RequiresExternalSampler() && frame.BitDepth() == 8u) {
    DCHECK_EQ(frame.NumTextures(), 1u);
    return media::PIXEL_FORMAT_XRGB;
  }

  switch (frame.format()) {
    case media::PIXEL_FORMAT_I420:
    case media::PIXEL_FORMAT_I420A:
    case media::PIXEL_FORMAT_I422:
    case media::PIXEL_FORMAT_I444:
    case media::PIXEL_FORMAT_XBGR:
    case media::PIXEL_FORMAT_ABGR:
    case media::PIXEL_FORMAT_XRGB:
    case media::PIXEL_FORMAT_ARGB:
    case media::PIXEL_FORMAT_NV12:
      break;
    default:
      return absl::nullopt;
  }

  if (mappable) {
    DCHECK_EQ(frame.layout().num_planes(),
              media::VideoFrame::NumPlanes(frame.format()));
    return frame.format();
  }

  // For legacy shared image formats, readback only works when planes and
  // textures are 1:1.
  if (frame.shared_image_format_type() ==
          media::SharedImageFormatType::kLegacy &&
      frame.NumTextures() != media::VideoFrame::NumPlanes(frame.format())) {
    return absl::nullopt;
  }

  return frame.format();
}

void CopyMappablePlanes(const media::VideoFrame& src_frame,
                        const gfx::Rect& src_rect,
                        const VideoFrameLayout& dest_layout,
                        base::span<uint8_t> dest_buffer) {
  for (wtf_size_t i = 0; i < dest_layout.NumPlanes(); i++) {
    const gfx::Size sample_size =
        media::VideoFrame::SampleSize(dest_layout.Format(), i);
    const int sample_bytes =
        media::VideoFrame::BytesPerElement(dest_layout.Format(), i);
    const uint8_t* src =
        src_frame.data(i) +
        src_rect.y() / sample_size.height() * src_frame.stride(i) +
        src_rect.x() / sample_size.width() * sample_bytes;
    libyuv::CopyPlane(src, static_cast<int>(src_frame.stride(i)),
                      dest_buffer.data() + dest_layout.Offset(i),
                      static_cast<int>(dest_layout.Stride(i)),
                      src_rect.width() / sample_size.width() * sample_bytes,
                      src_rect.height() / sample_size.height());
  }
}

bool CopyTexturablePlanes(media::VideoFrame& src_frame,
                          const gfx::Rect& src_rect,
                          const VideoFrameLayout& dest_layout,
                          base::span<uint8_t> dest_buffer) {
  auto wrapper = SharedGpuContext::ContextProviderWrapper();
  if (!wrapper)
    return false;

  auto* provider = wrapper->ContextProvider();
  if (!provider)
    return false;

  auto* ri = provider->RasterInterface();
  auto* gr_context = provider->GetGrContext();
  if (!ri)
    return false;

  for (wtf_size_t i = 0; i < dest_layout.NumPlanes(); i++) {
    const gfx::Size sample_size =
        media::VideoFrame::SampleSize(dest_layout.Format(), i);
    gfx::Rect plane_src_rect(src_rect.x() / sample_size.width(),
                             src_rect.y() / sample_size.height(),
                             src_rect.width() / sample_size.width(),
                             src_rect.height() / sample_size.height());
    uint8_t* dest_pixels = dest_buffer.data() + dest_layout.Offset(i);
    if (!media::ReadbackTexturePlaneToMemorySync(
            src_frame, i, plane_src_rect, dest_pixels, dest_layout.Stride(i),
            ri, gr_context, provider->GetCapabilities())) {
      // It's possible to fail after copying some but not all planes, leaving
      // the output buffer in a corrupt state D:
      return false;
    }
  }

  return true;
}

bool ParseCopyToOptions(const media::VideoFrame& frame,
                        VideoFrameCopyToOptions* options,
                        ExceptionState& exception_state,
                        VideoFrameLayout* dest_layout_out,
                        gfx::Rect* src_rect_out = nullptr) {
  DCHECK(dest_layout_out);

  auto copy_to_format = CopyToFormat(frame);
  if (!copy_to_format) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "Operation is not supported when format is null.");
    return false;
  }
  absl::optional<V8VideoPixelFormat> v8_format =
      ToV8VideoPixelFormat(*copy_to_format);

  gfx::Rect src_rect = frame.visible_rect();
  if (options->hasRect()) {
    src_rect =
        ToGfxRect(options->rect(), "rect", frame.coded_size(), exception_state);
    if (exception_state.HadException())
      return false;
  }
  if (!ValidateCropAlignment(*copy_to_format, (*v8_format).AsCStr(), src_rect,
                             options->hasRect() ? "rect" : "visibleRect",
                             exception_state)) {
    return false;
  }

  gfx::Size dest_coded_size = src_rect.size();
  VideoFrameLayout dest_layout(*copy_to_format, dest_coded_size,
                               exception_state);
  if (exception_state.HadException())
    return false;
  if (options->hasLayout()) {
    dest_layout = VideoFrameLayout(*copy_to_format, dest_coded_size,
                                   options->layout(), exception_state);
    if (exception_state.HadException())
      return false;
  }

  *dest_layout_out = dest_layout;
  if (src_rect_out)
    *src_rect_out = src_rect;
  return true;
}

// Convert and return |dest_layout|.
HeapVector<Member<PlaneLayout>> ConvertLayout(
    const VideoFrameLayout& dest_layout) {
  HeapVector<Member<PlaneLayout>> result;
  for (wtf_size_t i = 0; i < dest_layout.NumPlanes(); i++) {
    auto* plane = MakeGarbageCollected<PlaneLayout>();
    plane->setOffset(dest_layout.Offset(i));
    plane->setStride(dest_layout.Stride(i));
    result.push_back(plane);
  }
  return result;
}

}  // namespace

VideoFrame::VideoFrame(scoped_refptr<media::VideoFrame> frame,
                       ExecutionContext* context,
                       std::string monitoring_source_id,
                       sk_sp<SkImage> sk_image) {
  DCHECK(frame);
  handle_ = base::MakeRefCounted<VideoFrameHandle>(
      frame, std::move(sk_image), context, std::move(monitoring_source_id));
  external_allocated_memory_ =
      media::VideoFrame::AllocationSize(frame->format(), frame->coded_size());
  context->GetIsolate()->AdjustAmountOfExternalAllocatedMemory(
      external_allocated_memory_);
}

VideoFrame::VideoFrame(scoped_refptr<VideoFrameHandle> handle)
    : handle_(std::move(handle)) {
  DCHECK(handle_);

  // The provided |handle| may be invalid if close() was called while
  // it was being sent to another thread.
  auto local_frame = handle_->frame();
  if (!local_frame)
    return;

  external_allocated_memory_ = media::VideoFrame::AllocationSize(
      local_frame->format(), local_frame->coded_size());
  v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
      external_allocated_memory_);
}

VideoFrame::~VideoFrame() {
  ResetExternalMemory();
}

// static
VideoFrame* VideoFrame::Create(ScriptState* script_state,
                               const V8CanvasImageSource* source,
                               const VideoFrameInit* init,
                               ExceptionState& exception_state) {
  auto* image_source = ToCanvasImageSource(source, exception_state);
  if (!image_source) {
    // ToCanvasImageSource() will throw a source appropriate exception.
    return nullptr;
  }

  if (image_source->WouldTaintOrigin()) {
    exception_state.ThrowSecurityError(
        "VideoFrames can't be created from tainted sources.");
    return nullptr;
  }

  constexpr char kAlphaDiscard[] = "discard";

  // Special case <video> and VideoFrame to directly use the underlying frame.
  if (source->IsVideoFrame() || source->IsHTMLVideoElement()) {
    scoped_refptr<media::VideoFrame> source_frame;
    switch (source->GetContentType()) {
      case V8CanvasImageSource::ContentType::kVideoFrame:
        source_frame = source->GetAsVideoFrame()->frame();
        break;
      case V8CanvasImageSource::ContentType::kHTMLVideoElement:
        if (auto* wmp = source->GetAsHTMLVideoElement()->GetWebMediaPlayer())
          source_frame = wmp->GetCurrentFrameThenUpdate();
        break;
      default:
        NOTREACHED();
    }

    if (!source_frame) {
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        "Invalid source state");
      return nullptr;
    }

    const bool force_opaque = init->alpha() == kAlphaDiscard &&
                              !media::IsOpaque(source_frame->format());

    const auto wrapped_format =
        force_opaque ? ToOpaqueMediaPixelFormat(source_frame->format())
                     : source_frame->format();
    const gfx::Size& coded_size = source_frame->coded_size();
    const gfx::Rect default_visible_rect = source_frame->visible_rect();
    const gfx::Size default_display_size = source_frame->natural_size();
    ParsedVideoFrameInit parsed_init(init, wrapped_format, coded_size,
                                     default_visible_rect, default_display_size,
                                     exception_state);
    if (exception_state.HadException())
      return nullptr;

    // We can't modify frame metadata directly since there may be other owners
    // accessing these fields concurrently.
    if (init->hasTimestamp() || init->hasDuration() || force_opaque ||
        init->hasVisibleRect() || init->hasDisplayWidth()) {
      auto wrapped_frame = media::VideoFrame::WrapVideoFrame(
          source_frame, wrapped_format, parsed_init.visible_rect,
          parsed_init.display_size);
      if (!wrapped_frame) {
        exception_state.ThrowDOMException(
            DOMExceptionCode::kOperationError,
            String::Format("Failed to create a VideoFrame from "
                           "CanvasImageSource with format: %s, "
                           "coded size: %s, visibleRect: %s, display size: %s.",
                           VideoPixelFormatToString(wrapped_format).c_str(),
                           source_frame->coded_size().ToString().c_str(),
                           parsed_init.visible_rect.ToString().c_str(),
                           parsed_init.display_size.ToString().c_str()));
        return nullptr;
      }

      wrapped_frame->set_color_space(source_frame->ColorSpace());
      if (init->hasTimestamp()) {
        wrapped_frame->set_timestamp(base::Microseconds(init->timestamp()));
      }
      if (init->hasDuration()) {
        wrapped_frame->metadata().frame_duration =
            base::Microseconds(init->duration());
      }
      source_frame = std::move(wrapped_frame);
    }

    // Re-use the sk_image if available and not obsoleted by metadata overrides.
    sk_sp<SkImage> sk_image;
    if (source->GetContentType() ==
        V8CanvasImageSource::ContentType::kVideoFrame) {
      auto local_handle =
          source->GetAsVideoFrame()->handle()->CloneForInternalUse();
      // Note: It's possible for another realm (Worker) to destroy our handle if
      // this frame was transferred via BroadcastChannel to multiple realms.
      if (local_handle && local_handle->sk_image() && !force_opaque &&
          !init->hasVisibleRect() && !init->hasDisplayWidth() &&
          !init->hasDisplayHeight()) {
        sk_image = local_handle->sk_image();
      }
    }

    return MakeGarbageCollected<VideoFrame>(
        std::move(source_frame), ExecutionContext::From(script_state),
        /* monitoring_source_id */ std::string(), std::move(sk_image));
  }

  // Some elements like OffscreenCanvas won't choose a default size, so we must
  // ask them what size they think they are first.
  auto source_size =
      image_source->ElementSize(gfx::SizeF(), kRespectImageOrientation);

  SourceImageStatus status = kInvalidSourceImageStatus;
  auto image = image_source->GetSourceImageForCanvas(
      CanvasResourceProvider::FlushReason::kCreateVideoFrame, &status,
      source_size);
  if (!image || status != kNormalSourceImageStatus) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid source state");
    return nullptr;
  }

  const auto timestamp = base::Microseconds(
      (init && init->hasTimestamp()) ? init->timestamp() : 0);
  if (!init || !init->hasTimestamp()) {
    exception_state.ThrowTypeError("VideoFrameInit must provide timestamp");
    return nullptr;
  }

  const auto paint_image = image->PaintImageForCurrentFrame();
  const auto sk_image_info = paint_image.GetSkImageInfo();
  auto sk_color_space = sk_image_info.refColorSpace();
  if (!sk_color_space)
    sk_color_space = SkColorSpace::MakeSRGB();

  auto gfx_color_space = gfx::ColorSpace(*sk_color_space);
  if (!gfx_color_space.IsValid()) {
    exception_state.ThrowTypeError("Invalid color space");
    return nullptr;
  }

  const auto orientation = image->CurrentFrameOrientation().Orientation();
  const gfx::Size coded_size(sk_image_info.width(), sk_image_info.height());
  const gfx::Rect default_visible_rect(coded_size);
  const gfx::Size default_display_size(coded_size);

  sk_sp<SkImage> sk_image;
  scoped_refptr<media::VideoFrame> frame;
  if (image->IsTextureBacked() && SharedGpuContext::IsGpuCompositingEnabled()) {
    DCHECK(image->IsStaticBitmapImage());
    const auto format = media::VideoPixelFormatFromSkColorType(
        paint_image.GetColorType(),
        image->CurrentFrameKnownToBeOpaque() || init->alpha() == kAlphaDiscard);

    ParsedVideoFrameInit parsed_init(init, format, coded_size,
                                     default_visible_rect, default_display_size,
                                     exception_state);
    if (exception_state.HadException())
      return nullptr;

    auto* sbi = static_cast<StaticBitmapImage*>(image.get());
    gpu::MailboxHolder mailbox_holders[media::VideoFrame::kMaxPlanes] = {
        sbi->GetMailboxHolder()};
    const bool is_origin_top_left = sbi->IsOriginTopLeft();

    // The sync token needs to be updated when |frame| is released, but
    // AcceleratedStaticBitmapImage::UpdateSyncToken() is not thread-safe.
    auto release_cb = base::BindPostTaskToCurrentDefault(
        ConvertToBaseOnceCallback(CrossThreadBindOnce(
            [](scoped_refptr<Image> image, const gpu::SyncToken& sync_token) {
              static_cast<StaticBitmapImage*>(image.get())
                  ->UpdateSyncToken(sync_token);
            },
            std::move(image))));

    frame = media::VideoFrame::WrapNativeTextures(
        format, mailbox_holders, std::move(release_cb), coded_size,
        parsed_init.visible_rect, parsed_init.display_size, timestamp);

    if (frame)
      frame->metadata().texture_origin_is_top_left = is_origin_top_left;

    // Note: We could add the StaticBitmapImage to the VideoFrameHandle so we
    // can round trip through VideoFrame back to canvas w/o any copies, but
    // this doesn't seem like a common use case.
  } else {
    // Note: The current PaintImage may be lazy generated, for simplicity, we
    // just ask Skia to rasterize the image for us.
    //
    // A potential optimization could use PaintImage::DecodeYuv() to decode
    // directly into a media::VideoFrame. This would improve VideoFrame from
    // <img> creation, but probably such users should be using ImageDecoder
    // directly.
    sk_image = paint_image.GetSwSkImage();
    if (!sk_image) {
      // Can happen if, for example, |paint_image| is texture-backed and the
      // context was lost.
      exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                        "Failed to create video frame");
      return nullptr;
    }
    if (sk_image->isLazyGenerated()) {
      sk_image = sk_image->makeRasterImage();
      if (!sk_image) {
        exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                          "Failed to create video frame");
        return nullptr;
      }
    }

    const bool force_opaque =
        init && init->alpha() == kAlphaDiscard && !sk_image->isOpaque();

    const auto format = media::VideoPixelFormatFromSkColorType(
        sk_image->colorType(), sk_image->isOpaque() || force_opaque);
    ParsedVideoFrameInit parsed_init(init, format, coded_size,
                                     default_visible_rect, default_display_size,
                                     exception_state);
    if (exception_state.HadException())
      return nullptr;

    frame = media::CreateFromSkImage(sk_image, parsed_init.visible_rect,
                                     parsed_init.display_size, timestamp,
                                     force_opaque);

    // Above format determination unfortunately uses a bit of internal knowledge
    // from CreateFromSkImage(). Make sure they stay in sync.
    DCHECK(!frame || frame->format() == format);

    // If |sk_image| isn't rendered identically to |frame|, don't pass it along
    // when creating the blink::VideoFrame below.
    if (force_opaque || parsed_init.visible_rect != default_visible_rect ||
        parsed_init.display_size != default_display_size) {
      sk_image.reset();
    }
  }

  if (!frame) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Failed to create video frame");
    return nullptr;
  }

  frame->set_color_space(gfx_color_space);
  if (init->hasDuration()) {
    frame->metadata().frame_duration = base::Microseconds(init->duration());
  }
  if (orientation != ImageOrientationEnum::kDefault) {
    frame->metadata().transformation =
        ImageOrientationToVideoTransformation(orientation);
  }
  return MakeGarbageCollected<VideoFrame>(
      base::MakeRefCounted<VideoFrameHandle>(
          std::move(frame), std::move(sk_image),
          ExecutionContext::From(script_state)));
}

VideoFrame* VideoFrame::Create(ScriptState* script_state,
                               const AllowSharedBufferSource* data,
                               const VideoFrameBufferInit* init,
                               ExceptionState& exception_state) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);

  // Handle format; the string was validated by the V8 binding.
  auto typed_fmt = V8VideoPixelFormat::Create(init->format());
  auto media_fmt = ToMediaPixelFormat(typed_fmt->AsEnum());

  // Validate coded size.
  uint32_t coded_width = init->codedWidth();
  uint32_t coded_height = init->codedHeight();
  if (coded_width == 0) {
    exception_state.ThrowTypeError("codedWidth must be nonzero.");
    return nullptr;
  }
  if (coded_height == 0) {
    exception_state.ThrowTypeError("codedHeight must be nonzero.");
    return nullptr;
  }
  if (coded_width > media::limits::kMaxDimension ||
      coded_height > media::limits::kMaxDimension ||
      coded_width * coded_height > media::limits::kMaxCanvas) {
    exception_state.ThrowTypeError(
        String::Format("Coded size %u x %u exceeds implementation limit.",
                       coded_width, coded_height));
    return nullptr;
  }
  const gfx::Size src_coded_size(static_cast<int>(coded_width),
                                 static_cast<int>(coded_height));

  // Validate visibleRect.
  gfx::Rect src_visible_rect(src_coded_size);
  if (init->hasVisibleRect()) {
    src_visible_rect = ToGfxRect(init->visibleRect(), "visibleRect",
                                 src_coded_size, exception_state);
    if (exception_state.HadException() ||
        !ValidateOffsetAlignment(media_fmt, src_visible_rect, "visibleRect",
                                 exception_state)) {
      return nullptr;
    }
  }

  // Validate layout.
  VideoFrameLayout src_layout(media_fmt, src_coded_size, exception_state);
  if (exception_state.HadException())
    return nullptr;
  if (init->hasLayout()) {
    src_layout = VideoFrameLayout(media_fmt, src_coded_size, init->layout(),
                                  exception_state);
    if (exception_state.HadException())
      return nullptr;
  }

  // Validate data.
  auto buffer = AsSpan<const uint8_t>(data);
  if (!buffer.data()) {
    exception_state.ThrowTypeError("data is detached.");
    return nullptr;
  }
  if (buffer.size() < src_layout.Size()) {
    exception_state.ThrowTypeError("data is not large enough.");
    return nullptr;
  }

  // Validate display (natural) size.
  gfx::Size display_size = src_visible_rect.size();
  if (init->hasDisplayWidth() || init->hasDisplayHeight()) {
    display_size = ParseAndValidateDisplaySize(init, exception_state);
    if (exception_state.HadException())
      return nullptr;
  }

  // Set up the copy to be minimally-sized.
  gfx::Rect crop = AlignCrop(media_fmt, src_visible_rect);
  gfx::Size dest_coded_size = crop.size();
  gfx::Rect dest_visible_rect = gfx::Rect(src_visible_rect.size());

  // Create a frame.
  const auto timestamp = base::Microseconds(init->timestamp());
  auto& frame_pool = CachedVideoFramePool::From(*execution_context);
  auto frame = frame_pool.CreateFrame(
      media_fmt, dest_coded_size, dest_visible_rect, display_size, timestamp);
  if (!frame) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kOperationError,
        String::Format("Failed to create a VideoFrame with format: %s, "
                       "coded size: %s, visibleRect: %s, display size: %s.",
                       VideoPixelFormatToString(media_fmt).c_str(),
                       dest_coded_size.ToString().c_str(),
                       dest_visible_rect.ToString().c_str(),
                       display_size.ToString().c_str()));
    return nullptr;
  }

  if (init->hasColorSpace()) {
    VideoColorSpace* video_color_space =
        MakeGarbageCollected<VideoColorSpace>(init->colorSpace());
    frame->set_color_space(video_color_space->ToGfxColorSpace());
  } else {
    // So far all WebCodecs YUV formats are planar, so this test works. That
    // might not be the case in the future.
    frame->set_color_space(media::IsYuvPlanar(media_fmt)
                               ? gfx::ColorSpace::CreateREC709()
                               : gfx::ColorSpace::CreateSRGB());
  }

  if (init->hasDuration()) {
    frame->metadata().frame_duration = base::Microseconds(init->duration());
  }

  // Copy planes.
  for (wtf_size_t i = 0; i < media::VideoFrame::NumPlanes(media_fmt); i++) {
    const gfx::Size sample_size = media::VideoFrame::SampleSize(media_fmt, i);
    const int sample_bytes = media::VideoFrame::BytesPerElement(media_fmt, i);
    const int row_bytes = crop.width() / sample_size.width() * sample_bytes;
    const int rows = crop.height() / sample_size.height();
    libyuv::CopyPlane(buffer.data() + src_layout.Offset(i),
                      static_cast<int>(src_layout.Stride(i)),
                      frame->writable_data(i),
                      static_cast<int>(frame->stride(i)), row_bytes, rows);
  }

  return MakeGarbageCollected<VideoFrame>(std::move(frame),
                                          ExecutionContext::From(script_state));
}

absl::optional<V8VideoPixelFormat> VideoFrame::format() const {
  auto local_frame = handle_->frame();
  if (!local_frame)
    return absl::nullopt;

  auto copy_to_format = CopyToFormat(*local_frame);
  if (!copy_to_format)
    return absl::nullopt;

  return ToV8VideoPixelFormat(*copy_to_format);
}

uint32_t VideoFrame::codedWidth() const {
  auto local_frame = handle_->frame();
  if (!local_frame)
    return 0;
  return local_frame->coded_size().width();
}

uint32_t VideoFrame::codedHeight() const {
  auto local_frame = handle_->frame();
  if (!local_frame)
    return 0;
  return local_frame->coded_size().height();
}

absl::optional<DOMRectReadOnly*> VideoFrame::codedRect() {
  auto local_frame = handle_->frame();
  if (!local_frame)
    return absl::nullopt;

  if (!coded_rect_) {
    coded_rect_ = MakeGarbageCollected<DOMRectReadOnly>(
        0, 0, local_frame->coded_size().width(),
        local_frame->coded_size().height());
  }
  return coded_rect_;
}

absl::optional<DOMRectReadOnly*> VideoFrame::visibleRect() {
  auto local_frame = handle_->frame();
  if (!local_frame)
    return absl::nullopt;

  if (!visible_rect_) {
    visible_rect_ = MakeGarbageCollected<DOMRectReadOnly>(
        local_frame->visible_rect().x(), local_frame->visible_rect().y(),
        local_frame->visible_rect().width(),
        local_frame->visible_rect().height());
  }
  return visible_rect_;
}

uint32_t VideoFrame::displayWidth() const {
  auto local_frame = handle_->frame();
  if (!local_frame)
    return 0;

  const auto transform =
      local_frame->metadata().transformation.value_or(media::kNoTransformation);
  if (transform == media::kNoTransformation ||
      transform.rotation == media::VIDEO_ROTATION_0 ||
      transform.rotation == media::VIDEO_ROTATION_180) {
    return local_frame->natural_size().width();
  }
  return local_frame->natural_size().height();
}

uint32_t VideoFrame::displayHeight() const {
  auto local_frame = handle_->frame();
  if (!local_frame)
    return 0;
  const auto transform =
      local_frame->metadata().transformation.value_or(media::kNoTransformation);
  if (transform == media::kNoTransformation ||
      transform.rotation == media::VIDEO_ROTATION_0 ||
      transform.rotation == media::VIDEO_ROTATION_180) {
    return local_frame->natural_size().height();
  }
  return local_frame->natural_size().width();
}

int64_t VideoFrame::timestamp() const {
  return handle_->timestamp().InMicroseconds();
}

absl::optional<uint64_t> VideoFrame::duration() const {
  if (auto duration = handle_->duration())
    return duration->InMicroseconds();
  return absl::nullopt;
}

VideoColorSpace* VideoFrame::colorSpace() {
  auto local_frame = handle_->frame();
  if (!local_frame) {
    if (!empty_color_space_)
      empty_color_space_ = MakeGarbageCollected<VideoColorSpace>();

    return empty_color_space_;
  }

  if (!color_space_) {
    color_space_ =
        MakeGarbageCollected<VideoColorSpace>(local_frame->ColorSpace());
  }
  return color_space_;
}

uint32_t VideoFrame::allocationSize(VideoFrameCopyToOptions* options,
                                    ExceptionState& exception_state) {
  auto local_frame = handle_->frame();
  if (!local_frame) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "VideoFrame is closed.");
    return 0;
  }

  VideoFrameLayout dest_layout;
  if (!ParseCopyToOptions(*local_frame, options, exception_state, &dest_layout))
    return 0;

  return dest_layout.Size();
}

ScriptPromiseResolver* VideoFrame::CopyToAsync(
    ScriptState* script_state,
    scoped_refptr<media::VideoFrame> frame,
    gfx::Rect src_rect,
    const AllowSharedBufferSource* destination,
    const VideoFrameLayout& dest_layout) {
  auto* background_readback =
      BackgroundReadback::From(*ExecutionContext::From(script_state));
  if (!background_readback)
    return nullptr;

  ArrayBufferContents contents = PinArrayBufferContent(destination);
  if (!contents.DataLength())
    return nullptr;

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  auto readback_done_handler = [](ArrayBufferContents contents,
                                  ScriptPromiseResolver* resolver,
                                  VideoFrameLayout dest_layout, bool success) {
    auto* script_state = resolver->GetScriptState();
    if (success && script_state->ContextIsValid()) {
      resolver->Resolve(ConvertLayout(dest_layout));
    } else {
      resolver->Reject();
    }
  };
  auto done_cb = WTF::BindOnce(readback_done_handler, std::move(contents),
                               WrapPersistent(resolver), dest_layout);

  auto buffer = AsSpan<uint8_t>(destination);
  background_readback->ReadbackTextureBackedFrameToBuffer(
      std::move(frame), src_rect, dest_layout, buffer, std::move(done_cb));
  return resolver;
}

ScriptPromise VideoFrame::copyTo(ScriptState* script_state,
                                 const AllowSharedBufferSource* destination,
                                 VideoFrameCopyToOptions* options,
                                 ExceptionState& exception_state) {
  auto local_frame = handle_->frame();
  if (!local_frame) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot copy closed VideoFrame.");
    return ScriptPromise();
  }

  VideoFrameLayout dest_layout;
  gfx::Rect src_rect;
  if (!ParseCopyToOptions(*local_frame, options, exception_state, &dest_layout,
                          &src_rect)) {
    return ScriptPromise();
  }

  // Validate destination buffer.
  auto buffer = AsSpan<uint8_t>(destination);
  if (!buffer.data()) {
    exception_state.ThrowTypeError("destination is detached.");
    return ScriptPromise();
  }
  if (buffer.size() < dest_layout.Size()) {
    exception_state.ThrowTypeError("destination is not large enough.");
    return ScriptPromise();
  }

  // Copy planes.
  if (local_frame->IsMappable()) {
    CopyMappablePlanes(*local_frame, src_rect, dest_layout, buffer);
  } else if (local_frame->HasGpuMemoryBuffer()) {
    auto mapped_frame = media::ConvertToMemoryMappedFrame(local_frame);
    if (!mapped_frame) {
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        "Failed to read VideoFrame data.");
      return ScriptPromise();
    }
    CopyMappablePlanes(*mapped_frame, src_rect, dest_layout, buffer);
  } else {
    DCHECK(local_frame->HasTextures());

    if (auto* resolver = CopyToAsync(script_state, local_frame, src_rect,
                                     destination, dest_layout)) {
      return resolver->Promise();
    }

    if (!CopyTexturablePlanes(*local_frame, src_rect, dest_layout, buffer)) {
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        "Failed to read VideoFrame data.");
      return ScriptPromise();
    }
  }

  auto result = ConvertLayout(dest_layout);
  return ScriptPromise::Cast(
      script_state,
      ToV8Traits<IDLSequence<PlaneLayout>>::ToV8(script_state, result)
          .ToLocalChecked());
}

void VideoFrame::close() {
  handle_->Invalidate();
  ResetExternalMemory();
}

VideoFrame* VideoFrame::clone(ExceptionState& exception_state) {
  auto handle = handle_->Clone();
  if (!handle) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot clone closed VideoFrame.");
    return nullptr;
  }

  return MakeGarbageCollected<VideoFrame>(std::move(handle));
}

scoped_refptr<Image> VideoFrame::GetSourceImageForCanvas(
    CanvasResourceProvider::FlushReason,
    SourceImageStatus* status,
    const gfx::SizeF&,
    const AlphaDisposition alpha_disposition) {
  // UnpremultiplyAlpha is not implemented yet.
  DCHECK_EQ(alpha_disposition, kPremultiplyAlpha);

  const auto local_handle = handle_->CloneForInternalUse();
  if (!local_handle) {
    DLOG(ERROR) << "GetSourceImageForCanvas() called for closed frame.";
    *status = kInvalidSourceImageStatus;
    return nullptr;
  }

  const auto orientation_enum = VideoTransformationToImageOrientation(
      local_handle->frame()->metadata().transformation.value_or(
          media::kNoTransformation));
  if (auto sk_img = local_handle->sk_image()) {
    *status = kNormalSourceImageStatus;
    return UnacceleratedStaticBitmapImage::Create(std::move(sk_img),
                                                  orientation_enum);
  }

  auto* execution_context =
      ExecutionContext::From(v8::Isolate::GetCurrent()->GetCurrentContext());
  auto& provider_cache = CanvasResourceProviderCache::From(*execution_context);

  // TODO(https://crbug.com/1341235): The choice of color type, alpha type, and
  // color space is inappropriate in many circumstances.
  const auto& resource_provider_size = local_handle->frame()->natural_size();
  const auto resource_provider_info =
      SkImageInfo::Make(gfx::SizeToSkISize(resource_provider_size),
                        kN32_SkColorType, kPremul_SkAlphaType, nullptr);
  auto* resource_provider =
      provider_cache.CreateProvider(resource_provider_info);

  const auto dest_rect = gfx::Rect(resource_provider_size);
  auto image = CreateImageFromVideoFrame(local_handle->frame(),
                                         /*allow_zero_copy_images=*/true,
                                         resource_provider,
                                         /*video_renderer=*/nullptr, dest_rect);
  if (!image) {
    *status = kInvalidSourceImageStatus;
    return nullptr;
  }

  *status = kNormalSourceImageStatus;
  return image;
}

bool VideoFrame::WouldTaintOrigin() const {
  // VideoFrames can't be created from untainted sources currently. If we ever
  // add that ability we will need a tainting signal on the VideoFrame itself.
  // One example would be allowing <video> elements to provide a VideoFrame.
  return false;
}

gfx::SizeF VideoFrame::ElementSize(
    const gfx::SizeF& default_object_size,
    const RespectImageOrientationEnum respect_orientation) const {
  // BitmapSourceSize() will always ignore orientation.
  if (respect_orientation == kRespectImageOrientation) {
    auto local_frame = handle_->frame();
    if (!local_frame)
      return gfx::SizeF();

    const auto orientation_enum = VideoTransformationToImageOrientation(
        local_frame->metadata().transformation.value_or(
            media::kNoTransformation));
    auto orientation_adjusted_size = gfx::SizeF(local_frame->natural_size());
    if (ImageOrientation(orientation_enum).UsesWidthAsHeight())
      orientation_adjusted_size.Transpose();
    return orientation_adjusted_size;
  }
  return gfx::SizeF(BitmapSourceSize());
}

bool VideoFrame::IsVideoFrame() const {
  return true;
}

bool VideoFrame::IsOpaque() const {
  if (auto local_frame = handle_->frame())
    return media::IsOpaque(local_frame->format());
  return false;
}

bool VideoFrame::IsAccelerated() const {
  if (auto local_handle = handle_->CloneForInternalUse()) {
    return handle_->sk_image() ? false
                               : WillCreateAcceleratedImagesFromVideoFrame(
                                     local_handle->frame().get());
  }
  return false;
}

void VideoFrame::ResetExternalMemory() {
  if (external_allocated_memory_) {
    v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
        -external_allocated_memory_);
    external_allocated_memory_ = 0;
  }
}

gfx::Size VideoFrame::BitmapSourceSize() const {
  auto local_frame = handle_->frame();
  if (!local_frame)
    return gfx::Size();

  // ImageBitmaps should always return the size w/o respecting orientation.
  return local_frame->natural_size();
}

ScriptPromise VideoFrame::CreateImageBitmap(ScriptState* script_state,
                                            absl::optional<gfx::Rect> crop_rect,
                                            const ImageBitmapOptions* options,
                                            ExceptionState& exception_state) {
  const auto local_handle = handle_->CloneForInternalUse();
  if (!local_handle) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Cannot create ImageBitmap from closed VideoFrame.");
    return ScriptPromise();
  }

  // SkImages are always immutable, so we don't actually need to make a copy of
  // the image to satisfy the ImageBitmap spec.
  const auto orientation_enum = VideoTransformationToImageOrientation(
      local_handle->frame()->metadata().transformation.value_or(
          media::kNoTransformation));
  if (auto sk_img = local_handle->sk_image()) {
    auto* image_bitmap = MakeGarbageCollected<ImageBitmap>(
        UnacceleratedStaticBitmapImage::Create(std::move(sk_img),
                                               orientation_enum),
        crop_rect, options);
    return ImageBitmapSource::FulfillImageBitmap(script_state, image_bitmap,
                                                 options, exception_state);
  }

  auto* execution_context =
      ExecutionContext::From(v8::Isolate::GetCurrent()->GetCurrentContext());
  auto& provider_cache = CanvasResourceProviderCache::From(*execution_context);

  // TODO(https://crbug.com/1341235): The choice of color type, alpha type, and
  // color space is inappropriate in many circumstances.
  const auto& resource_provider_size = local_handle->frame()->natural_size();
  const auto resource_provider_info =
      SkImageInfo::Make(gfx::SizeToSkISize(resource_provider_size),
                        kN32_SkColorType, kPremul_SkAlphaType, nullptr);
  auto* resource_provider =
      provider_cache.CreateProvider(resource_provider_info);

  // We disable zero copy images since the ImageBitmap spec says created bitmaps
  // are copies. Many other paths can avoid doing this w/o issue, but hardware
  // decoders may have a limited number of outputs, so not making a copy becomes
  // an observable issues to clients.
  const auto dest_rect = gfx::Rect(resource_provider_size);
  auto image = CreateImageFromVideoFrame(local_handle->frame(),
                                         /*allow_zero_copy_images=*/false,
                                         resource_provider,
                                         /*video_renderer=*/nullptr, dest_rect);
  if (!image) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        String(("Unsupported VideoFrame: " +
                local_handle->frame()->AsHumanReadableString())
                   .c_str()));
    return ScriptPromise();
  }

  auto* image_bitmap =
      MakeGarbageCollected<ImageBitmap>(image, crop_rect, options);
  return ImageBitmapSource::FulfillImageBitmap(script_state, image_bitmap,
                                               options, exception_state);
}

void VideoFrame::Trace(Visitor* visitor) const {
  visitor->Trace(coded_rect_);
  visitor->Trace(visible_rect_);
  visitor->Trace(color_space_);
  visitor->Trace(empty_color_space_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
