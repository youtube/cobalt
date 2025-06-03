/*
 * Copyright (C) 2004, 2006, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_HTML_CANVAS_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_HTML_CANVAS_ELEMENT_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_blob_callback.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context_host.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap_source.h"
#include "third_party/blink/renderer/core/page/page_visibility_observer.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_host.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types_3d.h"
#include "third_party/blink/renderer/platform/graphics/offscreen_canvas_placeholder.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/surface_layer_bridge.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/prefinalizer.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"

#define CanvasDefaultInterpolationQuality kInterpolationLow

namespace blink {

class Canvas2DLayerBridge;
class CanvasContextCreationAttributesCore;
class CanvasDrawListener;
class CanvasHighDynamicRangeOptions;
class CanvasRenderingContext;
class CanvasRenderingContextFactory;
class CanvasResourceProvider;
class GraphicsContext;
class HTMLCanvasElement;
class ImageBitmapOptions;
class StaticBitmapImageToVideoFrameCopier;

class
    CanvasRenderingContext2DOrWebGLRenderingContextOrWebGL2RenderingContextOrImageBitmapRenderingContextOrGPUCanvasContext;
typedef CanvasRenderingContext2DOrWebGLRenderingContextOrWebGL2RenderingContextOrImageBitmapRenderingContextOrGPUCanvasContext
    RenderingContext;

CORE_EXPORT BASE_DECLARE_FEATURE(kStartCanvasWithAccelerationDisabled);

// This contains the information of HTML Canvas Element,
// There are four different types of rendering context this HTML Canvas can own.
// It can be a 3D Context (WebGL or WebGL2), 2D Context,
// BitmapRenderingContext or it can have no context (Offscreen placeholder).
// To check the no context case is good to check if there is a placeholder.
class CORE_EXPORT HTMLCanvasElement final
    : public HTMLElement,
      public ExecutionContextLifecycleObserver,
      public PageVisibilityObserver,
      public CanvasRenderingContextHost,
      public WebSurfaceLayerBridgeObserver,
      public ImageBitmapSource,
      public OffscreenCanvasPlaceholder {
  DEFINE_WRAPPERTYPEINFO();
  USING_PRE_FINALIZER(HTMLCanvasElement, Dispose);

 public:
  using Node::GetExecutionContext;

  explicit HTMLCanvasElement(Document&);
  ~HTMLCanvasElement() override;

  // Attributes and functions exposed to script
  unsigned width() const { return Size().width(); }
  unsigned height() const { return Size().height(); }

  void setWidth(unsigned, ExceptionState&);
  void setHeight(unsigned, ExceptionState&);

  void SetSize(gfx::Size new_size) final;

  // Called by Document::getCSSCanvasContext as well as above getContext().
  CanvasRenderingContext* GetCanvasRenderingContext(
      const String&,
      const CanvasContextCreationAttributesCore&);

  String toDataURL(const String& mime_type,
                   const ScriptValue& quality_argument,
                   ExceptionState&) const;
  String toDataURL(const String& mime_type,
                   ExceptionState& exception_state) const {
    return toDataURL(mime_type, ScriptValue(), exception_state);
  }

  void toBlob(V8BlobCallback*,
              const String& mime_type,
              const ScriptValue& quality_argument,
              ExceptionState&);
  void toBlob(V8BlobCallback* callback,
              const String& mime_type,
              ExceptionState& exception_state) {
    return toBlob(callback, mime_type, ScriptValue(), exception_state);
  }
  void configureHighDynamicRange(const CanvasHighDynamicRangeOptions*,
                                 ExceptionState&);

  bool IsPresentationAttribute(const QualifiedName&) const final;
  void CollectStyleForPresentationAttribute(const QualifiedName&,
                                            const AtomicString&,
                                            MutableCSSPropertyValueSet*) final;

  // Used for canvas capture.
  void AddListener(CanvasDrawListener*);
  void RemoveListener(CanvasDrawListener*);
  // Derived from OffscreenCanvasPlaceholder.
  bool HasCanvasCapture() const final { return !listeners_.empty(); }

  // Used for rendering
  void DidDraw(const SkIRect&) override;
  using CanvasRenderingContextHost::DidDraw;

  void Paint(GraphicsContext&,
             const PhysicalRect&,
             bool flatten_composited_layers);

  CanvasRenderingContext* RenderingContext() const override {
    return context_.Get();
  }

  bool OriginClean() const override;
  void SetOriginTainted() override { origin_clean_ = false; }

  Canvas2DLayerBridge* GetCanvas2DLayerBridge() {
    return canvas2d_bridge_.get();
  }
  Canvas2DLayerBridge* GetOrCreateCanvas2DLayerBridge();

  void DiscardResourceProvider() override;

  FontSelector* GetFontSelector() override;

  bool ShouldBeDirectComposited() const;

  const AtomicString ImageSourceURL() const override;

  InsertionNotificationRequest InsertedInto(ContainerNode&) override;

  bool IsDirty() { return !dirty_rect_.IsEmpty(); }

  void DoDeferredPaintInvalidation();

  void PreFinalizeFrame() override;
  void PostFinalizeFrame(FlushReason) override;

  CanvasResourceDispatcher* GetOrCreateResourceDispatcher() override;

  bool PushFrame(scoped_refptr<CanvasResource>&& image,
                 const SkIRect& damage_rect) override;

  // ExecutionContextLifecycleObserver and PageVisibilityObserver implementation
  void ContextDestroyed() override;

  // PageVisibilityObserver implementation
  void PageVisibilityChanged() override;

  // CanvasImageSource implementation
  scoped_refptr<Image> GetSourceImageForCanvas(
      FlushReason,
      SourceImageStatus*,
      const gfx::SizeF&,
      const AlphaDisposition alpha_disposition = kPremultiplyAlpha) override;
  bool WouldTaintOrigin() const override;
  gfx::SizeF ElementSize(const gfx::SizeF&,
                         const RespectImageOrientationEnum) const override;
  bool IsCanvasElement() const override { return true; }
  bool IsOpaque() const override;
  bool IsAccelerated() const override;

  // SurfaceLayerBridgeObserver implementation
  void OnWebLayerUpdated() override;
  void RegisterContentsLayer(cc::Layer*) override;
  void UnregisterContentsLayer(cc::Layer*) override;

  // CanvasResourceHost implementation
  bool IsPageVisible() override;
  void NotifyGpuContextLost() override;
  void SetNeedsCompositingUpdate() override;
  void UpdateMemoryUsage() override;
  size_t GetMemoryUsage() const override;
  bool ShouldAccelerate2dContext() const override;
  bool LowLatencyEnabled() const override;
  CanvasResourceProvider* GetOrCreateCanvasResourceProvider(
      RasterModeHint hint) override;
  bool IsPrinting() const override;
  void SetFilterQuality(cc::PaintFlags::FilterQuality filter_quality) override;
  bool IsHibernating() const override;
  void FlushRecording(FlushReason reason) override;

  // CanvasRenderingContextHost implementation.
  UkmParameters GetUkmParameters() override;

  void DisableAcceleration(std::unique_ptr<CanvasResourceProvider>
                               new_provider_for_testing = nullptr);

  // ImageBitmapSource implementation
  gfx::Size BitmapSourceSize() const override;
  ScriptPromise CreateImageBitmap(ScriptState*,
                                  absl::optional<gfx::Rect> crop_rect,
                                  const ImageBitmapOptions*,
                                  ExceptionState&) override;

  // OffscreenCanvasPlaceholder implementation.
  void SetOffscreenCanvasResource(scoped_refptr<CanvasResource>&&,
                                  viz::ResourceId resource_id) override;
  void Trace(Visitor*) const override;

  void SetResourceProviderForTesting(
      std::unique_ptr<CanvasResourceProvider> provider,
      std::unique_ptr<Canvas2DLayerBridge> bridge,
      const gfx::Size& size);

  static void RegisterRenderingContextFactory(
      std::unique_ptr<CanvasRenderingContextFactory>);

  bool StyleChangeNeedsDidDraw(const ComputedStyle* old_style,
                               const ComputedStyle& new_style);
  void StyleDidChange(const ComputedStyle* old_style,
                      const ComputedStyle& new_style);
  void LayoutObjectDestroyed();

  void NotifyListenersCanvasChanged();

  // For OffscreenCanvas that controls this html canvas element
  ::blink::SurfaceLayerBridge* SurfaceLayerBridge() const {
    return surface_layer_bridge_.get();
  }
  bool CreateLayer();

  void DetachContext() override { context_ = nullptr; }

  void WillDrawImageTo2DContext(CanvasImageSource*);

  ExecutionContext* GetTopExecutionContext() const override {
    return GetDocument().GetExecutionContext();
  }

  const KURL& GetExecutionContextUrl() const override {
    return GetDocument().Url();
  }

  DispatchEventResult HostDispatchEvent(Event* event) override {
    return DispatchEvent(*event);
  }

  void UpdateSuspendOffscreenCanvasAnimation();

  // Gets the settings of this Html Canvas Element. If there is a frame, it will
  // return the settings from the frame. If it is a frameless element it will
  // try to fetch the global dom window and get the settings from there.
  Settings* GetSettings() const;
  bool IsWebGL1Enabled() const override;
  bool IsWebGL2Enabled() const override;
  bool IsWebGLBlocked() const override;
  void SetContextCreationWasBlocked() override;

  bool NeedsUnbufferedInputEvents() const { return needs_unbuffered_input_; }

  void SetNeedsUnbufferedInputEvents(bool value) {
    needs_unbuffered_input_ = value;
  }

  scoped_refptr<StaticBitmapImage> Snapshot(FlushReason,
                                            SourceDrawingBuffer) const;

  // Returns the cc layer containing the contents. It's the cc layer of
  // SurfaceLayerBridge() or RenderingContext(), or nullptr if the canvas is not
  // composited.
  cc::Layer* ContentsCcLayer() const;

  // Return the image orientation setting from the layout object, if available.
  // In the absence of a layout object, kRespectImageOrientation will be
  // returned.
  RespectImageOrientationEnum RespectImageOrientation() const;

  bool IsCanvasClear() { return canvas_is_clear_; }

  bool IsPlaceholder() const override { return IsOffscreenCanvasRegistered(); }

  bool ShouldDisableAccelerationBecauseOfReadback() const;

 protected:
  void DidMoveToNewDocument(Document& old_document) override;
  void DidRecalcStyle(const StyleRecalcChange change) override;
  void RemovedFrom(ContainerNode& insertion_point) override;

 private:
  void Dispose();

  void ColorSchemeMayHaveChanged();

  void RecordIdentifiabilityMetric(IdentifiableSurface surface,
                                   IdentifiableToken value) const;

  // If the user is enrolled in the identifiability study, report the canvas
  // type, and if applicable, canvas digest, taint bits, and
  // |canvas_contents_token|, which represents the current bitmap displayed by
  // this canvas.
  void IdentifiabilityReportWithDigest(
      IdentifiableToken canvas_contents_token) const;

  void PaintInternal(GraphicsContext&, const PhysicalRect&);

  using ContextFactoryVector =
      Vector<std::unique_ptr<CanvasRenderingContextFactory>>;
  static ContextFactoryVector& RenderingContextFactories();
  static CanvasRenderingContextFactory* GetRenderingContextFactory(int);

  bool ShouldAccelerate() const;
  void ParseAttribute(const AttributeModificationParams&) override;
  LayoutObject* CreateLayoutObject(const ComputedStyle&) override;
  bool AreAuthorShadowsAllowed() const override { return false; }

  void Reset();

  std::unique_ptr<Canvas2DLayerBridge> Create2DLayerBridge();
  void SetCanvas2DLayerBridgeInternal(std::unique_ptr<Canvas2DLayerBridge>);

  void SetSurfaceSize(gfx::Size);

  bool PaintsIntoCanvasBuffer() const;

  String ToDataURLInternal(const String& mime_type,
                           const double& quality,
                           SourceDrawingBuffer) const;

  // Returns true if the canvas' context type is inherited from
  // ImageBitmapRenderingContextBase.
  bool HasImageBitmapContext() const;

  // Returns the transparent image resource for this canvas.
  scoped_refptr<StaticBitmapImage> GetTransparentImage();

  CanvasRenderingContext* GetCanvasRenderingContextInternal(
      const String&,
      const CanvasContextCreationAttributesCore&);

  scoped_refptr<StaticBitmapImage> GetSourceImageForCanvasInternal(
      FlushReason,
      SourceImageStatus*,
      const AlphaDisposition alpha_disposition = kPremultiplyAlpha);

  static std::pair<blink::Image*, float> BrokenCanvas(
      float device_scale_factor);

  FRIEND_TEST_ALL_PREFIXES(HTMLCanvasElementTest, BrokenCanvasHighRes);

  HeapHashSet<WeakMember<CanvasDrawListener>> listeners_;

  Member<CanvasRenderingContext> context_;
  // Used only for WebGL currently.
  bool context_creation_was_blocked_;

  bool disposing_ = false;
  bool canvas_is_clear_ = true;

  bool ignore_reset_;
  gfx::RectF dirty_rect_;

  bool origin_clean_;
  bool needs_unbuffered_input_ = false;
  bool style_is_visible_ = false;

  // It prevents repeated attempts in allocating resources after the first
  // attempt failed.
  bool HasResourceProvider() {
    return canvas2d_bridge_ || !!CanvasResourceHost::ResourceProvider();
  }

  // Canvas2DLayerBridge is used when canvas has 2d rendering context
  std::unique_ptr<Canvas2DLayerBridge> canvas2d_bridge_;
  void ReplaceExisting2dLayerBridge(
      std::unique_ptr<Canvas2DLayerBridge> new_layer_bridge,
      std::unique_ptr<CanvasResourceProvider> new_provider_for_testing =
          nullptr);

  // Used for OffscreenCanvas that controls this HTML canvas element
  // and for low latency mode.
  std::unique_ptr<::blink::SurfaceLayerBridge> surface_layer_bridge_;

  // Used for low latency mode.
  // TODO: rename to CanvasFrameDispatcher.
  std::unique_ptr<CanvasResourceDispatcher> frame_dispatcher_;

  std::unique_ptr<StaticBitmapImageToVideoFrameCopier> copier_;

  bool did_notify_listeners_for_current_frame_ = false;

  // GPU Memory Management
  mutable intptr_t externally_allocated_memory_;

  scoped_refptr<StaticBitmapImage> transparent_image_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_HTML_CANVAS_ELEMENT_H_
