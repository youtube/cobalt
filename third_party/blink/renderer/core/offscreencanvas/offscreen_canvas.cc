// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"

#include <memory>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metrics.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/renderer/core/css/css_font_selector.h"
#include "third_party/blink/renderer/core/css/offscreen_font_selector.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_async_blob_creator.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_context_creation_attributes_core.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context_factory.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_resource_tracker.h"
#include "third_party/blink/renderer/core/html/canvas/image_data.h"
#include "third_party/blink/renderer/core/html/canvas/ukm_parameters.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/workers/dedicated_worker_global_scope.h"
#include "third_party/blink/renderer/platform/bindings/no_alloc_direct_call_host.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_dispatcher.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/image-encoders/image_encoder_utils.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

OffscreenCanvas::OffscreenCanvas(ExecutionContext* context,
                                 const gfx::Size& size)
    : CanvasRenderingContextHost(
          CanvasRenderingContextHost::HostType::kOffscreenCanvasHost),
      execution_context_(context),
      size_(size) {
  // Other code in Blink watches for destruction of the context; be
  // robust here as well.
  if (!context->IsContextDestroyed()) {
    if (auto* window = DynamicTo<LocalDOMWindow>(context)) {
      // If this OffscreenCanvas is being created in the context of a
      // cross-origin iframe, it should prefer to use the low-power GPU.
      LocalFrame* frame = window->GetFrame();
      if (!(frame && frame->IsCrossOriginToOutermostMainFrame())) {
        AllowHighPerformancePowerPreference();
      }
    } else if (context->IsDedicatedWorkerGlobalScope()) {
      // Per spec, dedicated workers can only load same-origin top-level
      // scripts, so grant them access to the high-performance GPU.
      //
      // TODO(crbug.com/1050739): refine this logic. If the worker was
      // spawned from an iframe, keep track of whether that iframe was
      // itself cross-origin.
      AllowHighPerformancePowerPreference();
    }
  }

  CanvasResourceTracker::For(context->GetIsolate())->Add(this, context);
  UpdateMemoryUsage();
}

OffscreenCanvas* OffscreenCanvas::Create(ExecutionContext* context,
                                         unsigned width,
                                         unsigned height) {
  UMA_HISTOGRAM_BOOLEAN("Blink.OffscreenCanvas.NewOffscreenCanvas", true);
  return MakeGarbageCollected<OffscreenCanvas>(
      context, gfx::Size(ClampTo<int>(width), ClampTo<int>(height)));
}

OffscreenCanvas::~OffscreenCanvas() {
  v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
      -memory_usage_);
}

void OffscreenCanvas::Commit(scoped_refptr<CanvasResource>&& canvas_resource,
                             const SkIRect& damage_rect) {
  if (!HasPlaceholderCanvas() || !canvas_resource)
    return;
  RecordCanvasSizeToUMA(Size());

  base::TimeTicks commit_start_time = base::TimeTicks::Now();
  current_frame_damage_rect_.join(damage_rect);
  GetOrCreateResourceDispatcher()->DispatchFrameSync(
      std::move(canvas_resource), commit_start_time, current_frame_damage_rect_,
      !RenderingContext()->IsOriginTopLeft() /* needs_vertical_flip */,
      IsOpaque());
  current_frame_damage_rect_ = SkIRect::MakeEmpty();
}

void OffscreenCanvas::Dispose() {
  // We need to drop frame dispatcher, to prevent mojo calls from completing.
  disposing_ = true;
  frame_dispatcher_ = nullptr;
  DiscardResourceProvider();

  if (context_) {
    context_->DetachHost();
    context_ = nullptr;
  }
}

void OffscreenCanvas::DeregisterFromAnimationFrameProvider() {
  if (HasPlaceholderCanvas() && GetTopExecutionContext() &&
      GetTopExecutionContext()->IsDedicatedWorkerGlobalScope()) {
    WorkerAnimationFrameProvider* animation_frame_provider =
        To<DedicatedWorkerGlobalScope>(GetTopExecutionContext())
            ->GetAnimationFrameProvider();
    if (animation_frame_provider)
      animation_frame_provider->DeregisterOffscreenCanvas(this);
  }
}

void OffscreenCanvas::SetPlaceholderCanvasId(DOMNodeId canvas_id) {
  placeholder_canvas_id_ = canvas_id;
  if (GetTopExecutionContext() &&
      GetTopExecutionContext()->IsDedicatedWorkerGlobalScope()) {
    WorkerAnimationFrameProvider* animation_frame_provider =
        To<DedicatedWorkerGlobalScope>(GetTopExecutionContext())
            ->GetAnimationFrameProvider();
    DCHECK(animation_frame_provider);
    if (animation_frame_provider)
      animation_frame_provider->RegisterOffscreenCanvas(this);
  }
  if (frame_dispatcher_) {
    frame_dispatcher_->SetPlaceholderCanvasDispatcher(placeholder_canvas_id_);
  }
}

void OffscreenCanvas::setWidth(unsigned width) {
  gfx::Size new_size = size_;
  new_size.set_width(ClampTo<int>(width));
  SetSize(new_size);
}

void OffscreenCanvas::setHeight(unsigned height) {
  gfx::Size new_size = size_;
  new_size.set_height(ClampTo<int>(height));
  SetSize(new_size);
}

void OffscreenCanvas::SetSize(const gfx::Size& size) {
  // Setting size of a canvas also resets it.
  if (size == size_) {
    if (context_ && context_->IsRenderingContext2D()) {
      context_->Reset();
      origin_clean_ = true;
    }
    return;
  }

  size_ = size;
  UpdateMemoryUsage();
  current_frame_damage_rect_ = SkIRect::MakeWH(size_.width(), size_.height());

  if (frame_dispatcher_)
    frame_dispatcher_->Reshape(size_);
  if (context_) {
    if (context_->IsWebGL() || IsWebGPU()) {
      context_->Reshape(size_.width(), size_.height());
    } else if (context_->IsRenderingContext2D()) {
      context_->Reset();
      origin_clean_ = true;
    }
    context_->DidDraw(CanvasPerformanceMonitor::DrawType::kOther);
  }
}

ScriptPromise OffscreenCanvas::convertToBlob(ScriptState* script_state,
                                             const ImageEncodeOptions* options,
                                             ExceptionState& exception_state) {
  return CanvasRenderingContextHost::convertToBlob(script_state, options,
                                                   exception_state, context_);
}

void OffscreenCanvas::RecordTransfer() {
  UMA_HISTOGRAM_BOOLEAN("Blink.OffscreenCanvas.Transferred", true);
}

void OffscreenCanvas::SetNeutered() {
  DCHECK(!context_);
  is_neutered_ = true;
  size_.set_width(0);
  size_.set_height(0);
  DeregisterFromAnimationFrameProvider();
}

ImageBitmap* OffscreenCanvas::transferToImageBitmap(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (is_neutered_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Cannot transfer an ImageBitmap from a detached OffscreenCanvas");
    return nullptr;
  }
  if (!context_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot transfer an ImageBitmap from an "
                                      "OffscreenCanvas with no context");
    return nullptr;
  }

  ImageBitmap* image = context_->TransferToImageBitmap(script_state);
  if (!image) {
    // Undocumented exception (not in spec).
    exception_state.ThrowDOMException(DOMExceptionCode::kUnknownError,
                                      "ImageBitmap construction failed");
  }

  return image;
}

void OffscreenCanvas::RecordIdentifiabilityMetric(
    const blink::IdentifiableSurface& surface,
    const IdentifiableToken& token) const {
  if (!IdentifiabilityStudySettings::Get()->ShouldSampleSurface(surface))
    return;
  blink::IdentifiabilityMetricBuilder(GetExecutionContext()->UkmSourceID())
      .Add(surface, token)
      .Record(GetExecutionContext()->UkmRecorder());
}

scoped_refptr<Image> OffscreenCanvas::GetSourceImageForCanvas(
    CanvasResourceProvider::FlushReason reason,
    SourceImageStatus* status,
    const gfx::SizeF& size,
    const AlphaDisposition alpha_disposition) {
  if (!context_) {
    *status = kInvalidSourceImageStatus;
    sk_sp<SkSurface> surface =
        SkSurface::MakeRasterN32Premul(size_.width(), size_.height());
    return surface ? UnacceleratedStaticBitmapImage::Create(
                         surface->makeImageSnapshot())
                   : nullptr;
  }
  if (!size.width() || !size.height()) {
    *status = kZeroSizeCanvasSourceImageStatus;
    return nullptr;
  }
  scoped_refptr<StaticBitmapImage> image = context_->GetImage(reason);
  if (!image)
    image = CreateTransparentImage(Size());

  *status = image ? kNormalSourceImageStatus : kInvalidSourceImageStatus;

  // If the alpha_disposition is already correct, or the image is opaque, this
  // is a no-op.
  return GetImageWithAlphaDisposition(reason, std::move(image),
                                      alpha_disposition);
}

gfx::Size OffscreenCanvas::BitmapSourceSize() const {
  return size_;
}

ScriptPromise OffscreenCanvas::CreateImageBitmap(
    ScriptState* script_state,
    absl::optional<gfx::Rect> crop_rect,
    const ImageBitmapOptions* options,
    ExceptionState& exception_state) {
  if (context_) {
    context_->FinalizeFrame(
        CanvasResourceProvider::FlushReason::kCreateImageBitmap);
  }
  return ImageBitmapSource::FulfillImageBitmap(
      script_state,
      IsPaintable()
          ? MakeGarbageCollected<ImageBitmap>(this, crop_rect, options)
          : nullptr,
      options, exception_state);
}

bool OffscreenCanvas::IsOpaque() const {
  return context_ ? !context_->CreationAttributes().alpha : false;
}

CanvasRenderingContext* OffscreenCanvas::GetCanvasRenderingContext(
    ExecutionContext* execution_context,
    const String& id,
    const CanvasContextCreationAttributesCore& attributes) {
  DCHECK_EQ(execution_context, GetTopExecutionContext());

  if (execution_context->IsContextDestroyed())
    return nullptr;

  CanvasRenderingContext::CanvasRenderingAPI rendering_api =
      CanvasRenderingContext::RenderingAPIFromId(id, execution_context);

  // Unknown type.
  if (rendering_api == CanvasRenderingContext::CanvasRenderingAPI::kUnknown)
    return nullptr;

  if (auto* window = DynamicTo<LocalDOMWindow>(GetExecutionContext())) {
    if (attributes.color_space != PredefinedColorSpace::kSRGB)
      UseCounter::Count(window->document(), WebFeature::kCanvasUseColorSpace);
  }

  CanvasRenderingContextFactory* factory =
      GetRenderingContextFactory(static_cast<int>(rendering_api));
  if (!factory)
    return nullptr;

  if (context_) {
    if (context_->GetRenderingAPI() != rendering_api) {
      factory->OnError(
          this, "OffscreenCanvas has an existing context of a different type");
      return nullptr;
    }
  } else {
    CanvasContextCreationAttributesCore recomputed_attributes = attributes;
    if (!allow_high_performance_power_preference_)
      recomputed_attributes.power_preference = "low-power";

    context_ = factory->Create(this, recomputed_attributes);
    if (context_) {
      context_->RecordUKMCanvasRenderingAPI();
      context_->RecordUMACanvasRenderingAPI();
    }
  }

  return context_.Get();
}

OffscreenCanvas::ContextFactoryVector&
OffscreenCanvas::RenderingContextFactories() {
  DEFINE_STATIC_LOCAL(
      ContextFactoryVector, context_factories,
      (static_cast<int>(CanvasRenderingContext::CanvasRenderingAPI::kMaxValue) +
       1));
  return context_factories;
}

CanvasRenderingContextFactory* OffscreenCanvas::GetRenderingContextFactory(
    int type) {
  DCHECK_LE(type, static_cast<int>(
                      CanvasRenderingContext::CanvasRenderingAPI::kMaxValue));
  return RenderingContextFactories()[type].get();
}

void OffscreenCanvas::RegisterRenderingContextFactory(
    std::unique_ptr<CanvasRenderingContextFactory> rendering_context_factory) {
  CanvasRenderingContext::CanvasRenderingAPI rendering_api =
      rendering_context_factory->GetRenderingAPI();
  DCHECK_LE(rendering_api,
            CanvasRenderingContext::CanvasRenderingAPI::kMaxValue);
  DCHECK(!RenderingContextFactories()[static_cast<int>(rendering_api)]);
  RenderingContextFactories()[static_cast<int>(rendering_api)] =
      std::move(rendering_context_factory);
}

bool OffscreenCanvas::OriginClean() const {
  return origin_clean_ && !disable_reading_from_canvas_;
}

bool OffscreenCanvas::IsAccelerated() const {
  return context_ && context_->IsAccelerated();
}

bool OffscreenCanvas::HasPlaceholderCanvas() const {
  return placeholder_canvas_id_ != kInvalidDOMNodeId;
}

CanvasResourceDispatcher* OffscreenCanvas::GetOrCreateResourceDispatcher() {
  DCHECK(HasPlaceholderCanvas());
  // If we don't have a valid placeholder_canvas_id_, then this is a standalone
  // OffscreenCanvas, and it should not have a placeholder.
  if (!frame_dispatcher_) {
    scoped_refptr<base::SingleThreadTaskRunner>
        agent_group_scheduler_compositor_task_runner;
    scoped_refptr<base::SingleThreadTaskRunner> dispatcher_task_runner;
    if (auto* top_execution_context = GetTopExecutionContext()) {
      agent_group_scheduler_compositor_task_runner =
          top_execution_context->GetAgentGroupSchedulerCompositorTaskRunner();

      // AgentGroupSchedulerCompositorTaskRunner will be null for
      // SharedWorkers, but for windows and other workers it should be non-null.
      DCHECK(top_execution_context->IsSharedWorkerGlobalScope() ||
             agent_group_scheduler_compositor_task_runner);

      dispatcher_task_runner =
          top_execution_context->GetTaskRunner(TaskType::kInternalDefault);
    }

    // The frame dispatcher connects the current thread of OffscreenCanvas
    // (either main or worker) to the browser process and remains unchanged
    // throughout the lifetime of this OffscreenCanvas.
    frame_dispatcher_ = std::make_unique<CanvasResourceDispatcher>(
        this, std::move(dispatcher_task_runner),
        std::move(agent_group_scheduler_compositor_task_runner), client_id_,
        sink_id_, placeholder_canvas_id_, size_);

    if (HasPlaceholderCanvas())
      frame_dispatcher_->SetPlaceholderCanvasDispatcher(placeholder_canvas_id_);
  }
  return frame_dispatcher_.get();
}

CanvasResourceProvider* OffscreenCanvas::GetOrCreateResourceProvider() {
  if (ResourceProvider())
    return ResourceProvider();

  std::unique_ptr<CanvasResourceProvider> provider;
  gfx::Size surface_size(width(), height());
  const bool can_use_gpu =
      SharedGpuContext::IsGpuCompositingEnabled() &&
      (IsWebGL() || IsWebGPU() ||
       (IsRenderingContext2D() &&
        RuntimeEnabledFeatures::Accelerated2dCanvasEnabled() &&
        !(context_->CreationAttributes().will_read_frequently ==
          CanvasContextCreationAttributesCore::WillReadFrequently::kTrue)));
  const bool use_shared_image =
      can_use_gpu ||
      (HasPlaceholderCanvas() && SharedGpuContext::IsGpuCompositingEnabled());
  const bool use_scanout =
      use_shared_image && HasPlaceholderCanvas() &&
      (IsWebGPU() ||
       (IsWebGL() && RuntimeEnabledFeatures::WebGLImageChromiumEnabled()) ||
       (IsRenderingContext2D() &&
        RuntimeEnabledFeatures::Canvas2dImageChromiumEnabled()));

  uint32_t shared_image_usage_flags = gpu::SHARED_IMAGE_USAGE_DISPLAY_READ;
  if (use_scanout) {
    shared_image_usage_flags |= gpu::SHARED_IMAGE_USAGE_SCANOUT;
  }

  const SkImageInfo resource_info = SkImageInfo::Make(
      SkISize::Make(surface_size.width(), surface_size.height()),
      GetRenderingContextSkColorInfo());
  const cc::PaintFlags::FilterQuality filter_quality = FilterQuality();
  if (use_shared_image) {
    provider = CanvasResourceProvider::CreateSharedImageProvider(
        resource_info, filter_quality,
        CanvasResourceProvider::ShouldInitialize::kCallClear,
        SharedGpuContext::ContextProviderWrapper(),
        can_use_gpu ? RasterMode::kGPU : RasterMode::kCPU,
        false /*is_origin_top_left*/, shared_image_usage_flags);
  } else if (HasPlaceholderCanvas()) {
    // using the software compositor
    base::WeakPtr<CanvasResourceDispatcher> dispatcher_weakptr =
        GetOrCreateResourceDispatcher()->GetWeakPtr();
    provider = CanvasResourceProvider::CreateSharedBitmapProvider(
        resource_info, filter_quality,
        CanvasResourceProvider::ShouldInitialize::kCallClear,
        std::move(dispatcher_weakptr));
  }

  if (!provider) {
    // Last resort fallback is to use the bitmap provider. Using this
    // path is normal for software-rendered OffscreenCanvases that have no
    // placeholder canvas. If there is a placeholder, its content will not be
    // visible on screen, but at least readbacks will work. Failure to create
    // another type of resource prover above is a sign that the graphics
    // pipeline is in a bad state (e.g. gpu process crashed, out of memory)
    provider = CanvasResourceProvider::CreateBitmapProvider(
        resource_info, filter_quality,
        CanvasResourceProvider::ShouldInitialize::kCallClear);
  }

  ReplaceResourceProvider(std::move(provider));

  if (ResourceProvider() && ResourceProvider()->IsValid()) {
    // todo(crbug/1064363)  Add a separate UMA for Offscreen Canvas usage and
    // understand if the if (ResourceProvider() &&
    // ResourceProvider()->IsValid()) is really needed.
    base::UmaHistogramBoolean("Blink.Canvas.ResourceProviderIsAccelerated",
                              ResourceProvider()->IsAccelerated());
    base::UmaHistogramEnumeration("Blink.Canvas.ResourceProviderType",
                                  ResourceProvider()->GetType());
    DidDraw();

    if (needs_matrix_clip_restore_) {
      needs_matrix_clip_restore_ = false;
      context_->RestoreCanvasMatrixClipStack(ResourceProvider()->Canvas());
    }
  }
  return ResourceProvider();
}

void OffscreenCanvas::DidDraw(const SkIRect& rect) {
  if (rect.isEmpty())
    return;

  if (HasPlaceholderCanvas()) {
    needs_push_frame_ = true;
    if (!inside_worker_raf_)
      GetOrCreateResourceDispatcher()->SetNeedsBeginFrame(true);
  }
}

bool OffscreenCanvas::BeginFrame() {
  DCHECK(HasPlaceholderCanvas());
  GetOrCreateResourceDispatcher()->SetNeedsBeginFrame(false);
  return PushFrameIfNeeded();
}

void OffscreenCanvas::SetFilterQualityInResource(
    cc::PaintFlags::FilterQuality filter_quality) {
  if (FilterQuality() == filter_quality)
    return;

  SetFilterQuality(filter_quality);
  if (ResourceProvider())
    GetOrCreateResourceProvider()->SetFilterQuality(filter_quality);
}

bool OffscreenCanvas::PushFrameIfNeeded() {
  if (needs_push_frame_ && context_) {
    return context_->PushFrame();
  }
  return false;
}

bool OffscreenCanvas::PushFrame(scoped_refptr<CanvasResource>&& canvas_resource,
                                const SkIRect& damage_rect) {
  TRACE_EVENT0("blink", "OffscreenCanvas::PushFrame");
  DCHECK(needs_push_frame_);
  needs_push_frame_ = false;
  current_frame_damage_rect_.join(damage_rect);
  if (current_frame_damage_rect_.isEmpty() || !canvas_resource)
    return false;
  const base::TimeTicks commit_start_time = base::TimeTicks::Now();
  GetOrCreateResourceDispatcher()->DispatchFrame(
      std::move(canvas_resource), commit_start_time, current_frame_damage_rect_,
      !RenderingContext()->IsOriginTopLeft() /* needs_vertical_flip */,
      IsOpaque());
  current_frame_damage_rect_ = SkIRect::MakeEmpty();
  return true;
}

bool OffscreenCanvas::ShouldAccelerate2dContext() const {
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper =
      SharedGpuContext::ContextProviderWrapper();
  return context_provider_wrapper &&
         context_provider_wrapper->Utils()->Accelerated2DCanvasFeatureEnabled();
}

UkmParameters OffscreenCanvas::GetUkmParameters() {
  auto* context = GetExecutionContext();
  return {context->UkmRecorder(), context->UkmSourceID()};
}

void OffscreenCanvas::NotifyGpuContextLost() {
  if (context_ && !context_->isContextLost()) {
    // This code path is used only by 2D canvas, because NotifyGpuContextLost
    // is called by Canvas2DLayerBridge rather than the rendering context
    DCHECK(context_->IsRenderingContext2D());
    context_->LoseContext(CanvasRenderingContext::kRealLostContext);
  }
  if (frame_dispatcher_) {
    // We'll need to recreate a new frame dispatcher once the context is
    // restored in order to reestablish the compositor frame sink mojo
    // channel.
    frame_dispatcher_ = nullptr;
  }
}

FontSelector* OffscreenCanvas::GetFontSelector() {
  if (auto* window = DynamicTo<LocalDOMWindow>(GetExecutionContext())) {
    return window->document()->GetStyleEngine().GetFontSelector();
  }
  // TODO(crbug.com/1334864): Temporary mitigation.  Remove the following
  // CHECK once a more comprehensive solution has been implemented.
  CHECK(GetExecutionContext()->IsWorkerGlobalScope());
  return To<WorkerGlobalScope>(GetExecutionContext())->GetFontSelector();
}

void OffscreenCanvas::UpdateMemoryUsage() {
  int bytes_per_pixel = GetRenderingContextSkColorInfo().bytesPerPixel();

  base::CheckedNumeric<int32_t> memory_usage_checked = bytes_per_pixel;
  memory_usage_checked *= Size().width();
  memory_usage_checked *= Size().height();
  int32_t new_memory_usage =
      memory_usage_checked.ValueOrDefault(std::numeric_limits<int32_t>::max());

  // If the the rendering context supports NoAllocDirectCall, we must use a
  // deferrable action to update v8's externally allocated memory to avoid
  // triggering garbage collection while inside a FastAPICall scope.
  // TODO(junov): We assume that it is impossible to be inside a FastAPICall
  // from a host interface other than the rendering context.  This assumption
  // may need to be revisited in the future depending on how the usage of
  // [NoAllocDirectCall] evolves.
  intptr_t delta_bytes = new_memory_usage - memory_usage_;
  if (delta_bytes) {
    NoAllocDirectCallHost* nadc_host =
        context_ ? context_->AsNoAllocDirectCallHost() : nullptr;
    if (nadc_host) {
      nadc_host->PostDeferrableAction(WTF::BindOnce(
          [](intptr_t delta_bytes) {
            v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
                delta_bytes);
          },
          delta_bytes));
    } else {
      // Here we check "IsAllocationAllowed", but it is actually garbage
      // collection that is not allowed, and allocations can trigger GC.
      // AdjustAmountOfExternalAllocatedMemory is not an allocation but it
      // can trigger GC, So we use "IsAllocationAllowed" as a proxy for
      // "is GC allowed". When garbage collection is already in progress,
      // allocations are not allowed, but calling
      // AdjustAmountOfExternalAllocatedMemory is safe, hence the
      // 'diposing_' condition in the DCHECK below.
      DCHECK(ThreadState::Current()->IsAllocationAllowed() || disposing_);
      v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
          delta_bytes);
    }
    memory_usage_ = new_memory_usage;
  }
}

size_t OffscreenCanvas::GetMemoryUsage() const {
  return base::saturated_cast<size_t>(memory_usage_);
}

void OffscreenCanvas::Trace(Visitor* visitor) const {
  visitor->Trace(context_);
  visitor->Trace(execution_context_);
  EventTargetWithInlineData::Trace(visitor);
}

}  // namespace blink
