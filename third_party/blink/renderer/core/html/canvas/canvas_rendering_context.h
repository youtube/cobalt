/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_CANVAS_RENDERING_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_CANVAS_RENDERING_CONTEXT_H_

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "cc/paint/paint_flags.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_context_creation_attributes_core.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_performance_monitor.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context_host.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/graphics/canvas_color_params.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types_3d.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/prefinalizer.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/geometry/size.h"

namespace base {
struct PendingTask;
}  // namespace base

namespace cc {
class Layer;
class PaintCanvas;
}  // namespace cc

namespace gfx {
class ColorSpace;
}  // namespace gfx

namespace media {
class VideoFrame;
}  // namespace media

namespace blink {

class CanvasImageSource;
class CanvasResourceProvider;
class ComputedStyle;
class Document;
class Element;
class ExecutionContext;
class ImageBitmap;
class NoAllocDirectCallHost;
class ScriptState;
class StaticBitmapImage;
class
    V8UnionCanvasRenderingContext2DOrGPUCanvasContextOrImageBitmapRenderingContextOrWebGL2RenderingContextOrWebGLRenderingContext;
class
    V8UnionGPUCanvasContextOrImageBitmapRenderingContextOrOffscreenCanvasRenderingContext2DOrWebGL2RenderingContextOrWebGLRenderingContext;
class WebGraphicsContext3DVideoFramePool;

class CORE_EXPORT CanvasRenderingContext
    : public ScriptWrappable,
      public ActiveScriptWrappable<CanvasRenderingContext>,
      public Thread::TaskObserver {
  USING_PRE_FINALIZER(CanvasRenderingContext, Dispose);

 public:
  CanvasRenderingContext(const CanvasRenderingContext&) = delete;
  CanvasRenderingContext& operator=(const CanvasRenderingContext&) = delete;
  ~CanvasRenderingContext() override = default;

  // Correspond to CanvasRenderingAPI defined in
  // tools/metrics/histograms/enums.xml
  enum class CanvasRenderingAPI {
    kUnknown = -1,  // Not used by histogram.
    k2D = 0,
    kWebgl = 1,
    kWebgl2 = 2,
    kBitmaprenderer = 3,
    kWebgpu = 4,

    kMaxValue = kWebgpu,
  };

  CanvasRenderingAPI GetRenderingAPI() const { return canvas_rendering_type_; }

  bool IsRenderingContext2D() const {
    return canvas_rendering_type_ == CanvasRenderingAPI::k2D;
  }
  bool IsImageBitmapRenderingContext() const {
    return canvas_rendering_type_ == CanvasRenderingAPI::kBitmaprenderer;
  }
  bool IsWebGL() const {
    return canvas_rendering_type_ == CanvasRenderingAPI::kWebgl ||
           canvas_rendering_type_ == CanvasRenderingAPI::kWebgl2;
  }
  bool IsWebGL2() const {
    return canvas_rendering_type_ == CanvasRenderingAPI::kWebgl2;
  }
  bool IsWebGPU() const {
    return canvas_rendering_type_ == CanvasRenderingAPI::kWebgpu;
  }

  virtual NoAllocDirectCallHost* AsNoAllocDirectCallHost();

  // ActiveScriptWrappable
  // As this class inherits from ActiveScriptWrappable, as long as
  // HasPendingActivity returns true, we can ensure that the Garbage Collector
  // won't try to collect this class. This is needed specifically for the
  // offscreencanvas use case.
  bool HasPendingActivity() const override { return false; }
  ExecutionContext* GetExecutionContext() const {
    if (!Host())
      return nullptr;
    return Host()->GetTopExecutionContext();
  }

  void RecordUKMCanvasRenderingAPI();
  void RecordUMACanvasRenderingAPI();

  // This is only used in WebGL
  void RecordUKMCanvasDrawnToRenderingAPI();

  static CanvasRenderingAPI RenderingAPIFromId(
      const String& id,
      const ExecutionContext* execution_context);

  CanvasRenderingContextHost* Host() const { return host_; }
  virtual SkColorInfo CanvasRenderingContextSkColorInfo() const;

  virtual scoped_refptr<StaticBitmapImage> GetImage(
      CanvasResourceProvider::FlushReason) = 0;
  virtual bool IsComposited() const = 0;
  virtual bool IsAccelerated() const = 0;
  virtual bool IsOriginTopLeft() const {
    // Canvas contexts have the origin of coordinates on the top left corner.
    // Accelerated resources (e.g. GPU textures) have their origin of
    // coordinates in the upper left corner.
    return !IsAccelerated();
  }
  virtual bool ShouldAntialias() const { return false; }
  // Indicates whether the entire tab is backgrounded. Passing false
  // to this method may cause some canvas context implementations to
  // aggressively discard resources, which is not desired for canvases
  // which are being rendered to, just not being displayed in the
  // page.
  virtual void SetIsInHiddenPage(bool) = 0;
  // Indicates whether the canvas is being displayed in the page;
  // i.e., doesn't have display:none, and is visible. The initial
  // value for all context types is assumed to be false; this will be
  // called when the context is first displayed.
  virtual void SetIsBeingDisplayed(bool) = 0;
  virtual bool isContextLost() const { return true; }
  // TODO(fserb): remove AsV8RenderingContext and AsV8OffscreenRenderingContext.
  virtual V8UnionCanvasRenderingContext2DOrGPUCanvasContextOrImageBitmapRenderingContextOrWebGL2RenderingContextOrWebGLRenderingContext*
  AsV8RenderingContext() {
    NOTREACHED();
    return nullptr;
  }
  virtual V8UnionGPUCanvasContextOrImageBitmapRenderingContextOrOffscreenCanvasRenderingContext2DOrWebGL2RenderingContextOrWebGLRenderingContext*
  AsV8OffscreenRenderingContext() {
    NOTREACHED();
    return nullptr;
  }
  virtual bool IsPaintable() const = 0;
  void DidDraw(CanvasPerformanceMonitor::DrawType draw_type) {
    return DidDraw(Host() ? SkIRect::MakeWH(Host()->width(), Host()->height())
                          : SkIRect::MakeEmpty(),
                   draw_type);
  }
  void DidDraw(const SkIRect& dirty_rect, CanvasPerformanceMonitor::DrawType);

  // Return true if the content is updated.
  virtual bool PaintRenderingResultsToCanvas(SourceDrawingBuffer) {
    return false;
  }

  virtual bool CopyRenderingResultsFromDrawingBuffer(CanvasResourceProvider*,
                                                     SourceDrawingBuffer) {
    return false;
  }

  // Copy the contents of the rendering context to a media::VideoFrame created
  // using `frame_pool`, with color space specified by `dst_color_space`. If
  // successful, take (using std::move) `callback` and issue it with the
  // resulting frame, once the copy is completed. On failure, do not take
  // `callback`.
  using VideoFrameCopyCompletedCallback =
      base::OnceCallback<void(scoped_refptr<media::VideoFrame>)>;
  virtual bool CopyRenderingResultsToVideoFrame(
      WebGraphicsContext3DVideoFramePool* frame_pool,
      SourceDrawingBuffer,
      const gfx::ColorSpace& dst_color_space,
      VideoFrameCopyCompletedCallback callback) {
    return false;
  }

  virtual cc::Layer* CcLayer() const { return nullptr; }

  enum LostContextMode {
    kNotLostContext,

    // Lost context occurred at the graphics system level.
    kRealLostContext,

    // Lost context provoked by WEBGL_lose_context.
    kWebGLLoseContextLostContext,

    // Lost context occurred due to internal implementation reasons.
    kSyntheticLostContext,
  };
  virtual void LoseContext(LostContextMode) {}
  virtual void SendContextLostEventIfNeeded() {}

  // This method gets called at the end of script tasks that modified
  // the contents of the canvas (called didDraw). It marks the completion
  // of a presentable frame.
  virtual void FinalizeFrame(CanvasResourceProvider::FlushReason) {}

  // Thread::TaskObserver implementation
  void DidProcessTask(const base::PendingTask&) override;
  void WillProcessTask(const base::PendingTask&, bool) final {}

  // Canvas2D-specific interface
  virtual void RestoreCanvasMatrixClipStack(cc::PaintCanvas*) const {}
  virtual void Reset() {}
  virtual void ClearRect(double x, double y, double width, double height) {}
  virtual void DidSetSurfaceSize() {}
  virtual void SetShouldAntialias(bool) {}
  virtual void setFont(const String&) {}
  virtual void StyleDidChange(const ComputedStyle* old_style,
                              const ComputedStyle& new_style) {}
  virtual String GetIdFromControl(const Element* element) { return String(); }
  virtual void ResetUsageTracking() {}

  // WebGL-specific interface
  virtual bool UsingSwapChain() const { return false; }
  virtual void MarkLayerComposited() { NOTREACHED(); }
  virtual sk_sp<SkData> PaintRenderingResultsToDataArray(SourceDrawingBuffer) {
    NOTREACHED();
    return nullptr;
  }
  virtual gfx::Size DrawingBufferSize() const {
    NOTREACHED();
    return gfx::Size(0, 0);
  }

  // WebGL & WebGPU-specific interface
  virtual void SetHDRConfiguration(
      gfx::HDRMode hdr_mode,
      absl::optional<gfx::HDRMetadata> hdr_metadata) {}
  virtual void SetFilterQuality(cc::PaintFlags::FilterQuality) { NOTREACHED(); }
  virtual void Reshape(int width, int height) {}
  virtual int ExternallyAllocatedBufferCountPerPixel() {
    NOTREACHED();
    return 0;
  }

  // OffscreenCanvas-specific methods.
  virtual bool PushFrame() { return false; }
  virtual ImageBitmap* TransferToImageBitmap(ScriptState*) { return nullptr; }

  // Notification the color scheme of the HTMLCanvasElement may have changed.
  virtual void ColorSchemeMayHaveChanged() {}

  bool WouldTaintOrigin(CanvasImageSource*);
  void DidMoveToNewDocument(Document*);

  void DetachHost() { host_ = nullptr; }

  const CanvasContextCreationAttributesCore& CreationAttributes() const {
    return creation_attributes_;
  }

  void Trace(Visitor*) const override;
  virtual void Stop() = 0;

  virtual IdentifiableToken IdentifiableTextToken() const {
    // Token representing no bytes.
    return IdentifiableToken(base::span<const uint8_t>());
  }

  virtual bool IdentifiabilityEncounteredSkippedOps() const { return false; }

  virtual bool IdentifiabilityEncounteredSensitiveOps() const { return false; }

  static CanvasPerformanceMonitor& GetCanvasPerformanceMonitor();

  virtual bool IdentifiabilityEncounteredPartiallyDigestedImage() const {
    return false;
  }

  bool did_print_in_current_task() const { return did_print_in_current_task_; }

 protected:
  CanvasRenderingContext(CanvasRenderingContextHost*,
                         const CanvasContextCreationAttributesCore&,
                         CanvasRenderingAPI);

  virtual void Dispose();

 private:
  Member<CanvasRenderingContextHost> host_;
  CanvasColorParams color_params_;
  CanvasContextCreationAttributesCore creation_attributes_;

  void RenderTaskEnded();
  bool did_draw_in_current_task_ = false;
  bool did_print_in_current_task_ = false;

  const CanvasRenderingAPI canvas_rendering_type_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_CANVAS_RENDERING_CONTEXT_H_
