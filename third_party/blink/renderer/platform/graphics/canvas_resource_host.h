// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_HOST_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_HOST_H_

#include <memory>

#include "cc/layers/texture_layer.h"
#include "cc/layers/texture_layer_client.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/hdr_metadata.h"

namespace cc {
class PaintCanvas;
}  // namespace cc

namespace blink {

class CanvasResourceProvider;
class SharedContextRateLimiter;

class PLATFORM_EXPORT CanvasResourceHost : public cc::TextureLayerClient {
 public:
  explicit CanvasResourceHost(gfx::Size size);
  ~CanvasResourceHost() override;

  // cc::TextureLayerClient implementation.
  bool PrepareTransferableResource(
      viz::TransferableResource* out_resource,
      viz::ReleaseCallback* out_release_callback) override;

  virtual void NotifyGpuContextLost() = 0;
  virtual void SetNeedsCompositingUpdate() = 0;
  virtual void InitializeForRecording(cc::PaintCanvas* canvas) const = 0;
  virtual void UpdateMemoryUsage() = 0;
  virtual size_t GetMemoryUsage() const = 0;
  virtual void PageVisibilityChanged() {}
  virtual CanvasResourceProvider* GetOrCreateCanvasResourceProvider(
      RasterModeHint hint) = 0;
  virtual CanvasResourceProvider* GetOrCreateCanvasResourceProviderImpl(
      RasterModeHint hint) = 0;
  CanvasResourceProvider*
  GetOrCreateResourceProviderWithCurrentRasterModeHint() {
    return GetOrCreateCanvasResourceProvider(preferred_2d_raster_mode());
  }

  // Initialize the indicated cc::Layer with the HTMLCanvasElement's CSS
  // properties. This is a no-op if `this` is not an HTMLCanvasElement.
  virtual void InitializeLayerWithCSSProperties(cc::Layer* layer) {}

  bool IsComposited() const;
  bool IsResourceValid();
  virtual bool HasPlacedElements() const { return false; }
  gfx::Size Size() const { return size_; }
  virtual void SetSize(gfx::Size size) { size_ = size; }

  void SetHdrMetadata(const gfx::HDRMetadata& hdr_metadata);
  const gfx::HDRMetadata& GetHDRMetadata() const { return hdr_metadata_; }

  virtual bool LowLatencyEnabled() const { return false; }

  CanvasResourceProvider* ResourceProvider() const {
    return resource_provider_.get();
  }

  void FlushRecording(FlushReason reason);

  std::unique_ptr<CanvasResourceProvider> ReplaceResourceProvider(
      std::unique_ptr<CanvasResourceProvider>);

  virtual void DiscardResourceProvider();

  void SetIsDisplayed(bool);
  bool IsDisplayed() const { return is_displayed_; }

  virtual bool IsPageVisible() const = 0;

  virtual bool IsPrinting() const { return false; }
  virtual bool PrintedInCurrentTask() const = 0;
  virtual bool IsHibernating() const { return false; }

  RasterModeHint preferred_2d_raster_mode() const {
    return preferred_2d_raster_mode_;
  }

  bool ShouldTryToUseGpuRaster() const;
  void SetPreferred2DRasterMode(RasterModeHint);

  // Temporary plumbing while relocating code from Canvas2DLayerBridge.
  SharedContextRateLimiter* RateLimiter() const;
  void CreateRateLimiter();
  unsigned IncrementFramesSinceLastCommit() {
    return ++frames_since_last_commit_;
  }
  void AlwaysEnableRasterTimersForTesting() {
    always_enable_raster_timers_for_testing_ = true;
  }

  // Actual RasterMode used for rendering 2d primitives.
  RasterMode GetRasterMode() const;
  void ResetLayer();
  void ClearLayerTexture();
  void SetNeedsPushProperties();
  cc::TextureLayer* GetOrCreateCcLayerIfNeeded();
  cc::TextureLayer* CcLayer() { return cc_layer_.get(); }
  void DoPaintInvalidation(const gfx::Rect& dirty_rect);
  void SetOpacityMode(OpacityMode opacity_mode);

  // Temporary, for canvas_2d_layer_bridge use.
  bool context_lost() { return context_lost_; }
  void set_context_lost(bool value) { context_lost_ = value; }

  bool shared_bitmap_gpu_channel_lost() const {
    return shared_bitmap_gpu_channel_lost_;
  }
  void set_shared_bitmap_gpu_channel_lost(bool value) {
    shared_bitmap_gpu_channel_lost_ = value;
  }

  virtual void SetTransferToGPUTextureWasInvoked() {}
  virtual bool TransferToGPUTextureWasInvoked() { return false; }

 private:
  bool is_displayed_ = false;
  bool context_lost_ = false;
  bool shared_bitmap_gpu_channel_lost_ = false;
  unsigned frames_since_last_commit_ = 0;
  std::unique_ptr<SharedContextRateLimiter> rate_limiter_;
  std::unique_ptr<CanvasResourceProvider> resource_provider_;
  gfx::HDRMetadata hdr_metadata_;
  RasterModeHint preferred_2d_raster_mode_ = RasterModeHint::kPreferCPU;
  gfx::Size size_;
  bool always_enable_raster_timers_for_testing_ = false;
  // TODO(380896841): Extremely confusingly, this cc::TextureLayer, which in
  // this superclass of CanvasRenderingContextHost, is only used by 2D
  // canvases.
  scoped_refptr<cc::TextureLayer> cc_layer_;
  OpacityMode opacity_mode_ = kNonOpaque;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_HOST_H_
