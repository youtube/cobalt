// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_CANVAS_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_CANVAS_CONTEXT_H_

#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_canvas_alpha_mode.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_typedefs.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context_factory.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_swap_buffer_provider.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"

namespace blink {

class GPUDevice;
class GPUCanvasConfiguration;
class GPUSwapChain;
class GPUTexture;
class WebGPUTextureAlphaClearer;
class V8UnionHTMLCanvasElementOrOffscreenCanvas;

// A GPUCanvasContext does little by itself and basically just binds a canvas
// and a GPUSwapChain together and forwards calls from one to the other.
class GPUCanvasContext : public CanvasRenderingContext,
                         public WebGPUSwapBufferProvider::Client {
  DEFINE_WRAPPERTYPEINFO();

 public:
  class Factory : public CanvasRenderingContextFactory {
   public:
    Factory() = default;

    Factory(const Factory&) = delete;
    Factory& operator=(const Factory&) = delete;

    ~Factory() override;

    CanvasRenderingContext* Create(
        CanvasRenderingContextHost*,
        const CanvasContextCreationAttributesCore&) override;
    CanvasRenderingContext::CanvasRenderingAPI GetRenderingAPI() const override;
  };

  GPUCanvasContext(CanvasRenderingContextHost*,
                   const CanvasContextCreationAttributesCore&);

  GPUCanvasContext(const GPUCanvasContext&) = delete;
  GPUCanvasContext& operator=(const GPUCanvasContext&) = delete;

  ~GPUCanvasContext() override;

  void Trace(Visitor*) const override;

  // CanvasRenderingContext implementation
  V8RenderingContext* AsV8RenderingContext() final;
  V8OffscreenRenderingContext* AsV8OffscreenRenderingContext() final;
  SkColorInfo CanvasRenderingContextSkColorInfo() const override;
  // Produces a snapshot of the current contents of the swap chain if possible.
  // If that texture has already been sent to the compositor, will produce a
  // snapshot of the just released texture associated to this gpu context.
  // todo(crbug/1267243) Make snapshot always return the current frame.
  scoped_refptr<StaticBitmapImage> GetImage(
      CanvasResourceProvider::FlushReason) final;
  bool PaintRenderingResultsToCanvas(SourceDrawingBuffer) final;
  // Copies the back buffer to given shared image resource provider which must
  // be webgpu compatible. Returns true on success.
  bool CopyRenderingResultsFromDrawingBuffer(CanvasResourceProvider*,
                                             SourceDrawingBuffer) final;
  bool CopyRenderingResultsToVideoFrame(
      WebGraphicsContext3DVideoFramePool* frame_pool,
      SourceDrawingBuffer src_buffer,
      const gfx::ColorSpace& dst_color_space,
      VideoFrameCopyCompletedCallback callback) override;
  void SetIsInHiddenPage(bool) override {}
  void SetIsBeingDisplayed(bool) override {}
  bool isContextLost() const override { return false; }
  bool IsComposited() const final { return true; }
  bool IsAccelerated() const final { return true; }
  bool IsOriginTopLeft() const final { return true; }
  void SetFilterQuality(cc::PaintFlags::FilterQuality) override;
  bool IsPaintable() const final { return true; }
  int ExternallyAllocatedBufferCountPerPixel() final { return 1; }
  void Stop() final;
  cc::Layer* CcLayer() const final;
  void Reshape(int width, int height) override;

  // OffscreenCanvas-specific methods
  bool PushFrame() final;
  // Returns a StaticBitmapImage backed by a texture containing the current
  // contents of the front buffer. This is done without any pixel copies. The
  // texture in the ImageBitmap is from the active ContextProvider on the
  // WebGPUSwapBufferProvider.
  ImageBitmap* TransferToImageBitmap(ScriptState*) final;

  bool IsOffscreenCanvas() const {
    if (Host())
      return Host()->IsOffscreenCanvas();
    return false;
  }

  // gpu_presentation_context.idl
  V8UnionHTMLCanvasElementOrOffscreenCanvas* getHTMLOrOffscreenCanvas() const;

  void configure(const GPUCanvasConfiguration* descriptor, ExceptionState&);
  void unconfigure();
  GPUTexture* getCurrentTexture(ExceptionState&);

  // WebGPUSwapBufferProvider::Client implementation
  void OnTextureTransferred() override;

 private:
  void DetachSwapBuffers();
  void ReplaceDrawingBuffer(bool destroy_swap_buffers);
  void InitializeAlphaModePipeline(WGPUTextureFormat format);

  void FinalizeFrame(CanvasResourceProvider::FlushReason) override;

  scoped_refptr<StaticBitmapImage> SnapshotInternal(
      const WGPUTexture& texture,
      const gfx::Size& size) const;

  bool CopyTextureToResourceProvider(
      const WGPUTexture& texture,
      const gfx::Size& size,
      CanvasResourceProvider* resource_provider) const;

  // Can't use DawnObjectBase, because the device can be reconfigured.
  const DawnProcTable& GetProcs() const;
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> GetContextProviderWeakPtr()
      const;

  cc::PaintFlags::FilterQuality filter_quality_ =
      cc::PaintFlags::FilterQuality::kLow;
  Member<GPUDevice> device_;
  Member<GPUTexture> texture_;
  PredefinedColorSpace color_space_ = PredefinedColorSpace::kSRGB;
  V8GPUCanvasAlphaMode::Enum alpha_mode_;
  scoped_refptr<WebGPUTextureAlphaClearer> alpha_clearer_;
  scoped_refptr<WebGPUSwapBufferProvider> swap_buffers_;

  bool new_texture_required_ = true;
  bool stopped_ = false;

  // Matches [[configuration]] != null in the WebGPU specification.
  bool configured_ = false;
  // Matches [[texture_descriptor]] in the WebGPU specification except that it
  // never becomes null.
  WGPUTextureDescriptor texture_descriptor_;
  std::unique_ptr<WGPUTextureFormat[]> view_formats_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_CANVAS_CONTEXT_H_
