// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_TEST_FAKE_WEB_GRAPHICS_CONTEXT_3D_PROVIDER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_TEST_FAKE_WEB_GRAPHICS_CONTEXT_3D_PROVIDER_H_

#include "cc/test/stub_decode_cache.h"
#include "cc/tiles/image_decode_cache.h"
#include "components/viz/test/test_context_provider.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/raster_implementation_gles.h"
#include "gpu/command_buffer/client/webgpu_interface_stub.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/config/gpu_feature_info.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "third_party/skia/include/gpu/mock/GrMockTypes.h"

namespace blink {

class FakeWebGraphicsContext3DProvider : public WebGraphicsContext3DProvider {
 public:
  explicit FakeWebGraphicsContext3DProvider(
      gpu::gles2::GLES2Interface* gl,
      cc::ImageDecodeCache* cache = nullptr,
      GrDirectContext* gr_context = nullptr,
      viz::RasterContextProvider* raster_context_provider = nullptr)
      : gl_(gl),
        image_decode_cache_(cache ? cache : &stub_image_decode_cache_),
        raster_context_provider_(raster_context_provider) {
    if (gr_context) {
      gr_context_ = sk_ref_sp<GrDirectContext>(gr_context);
    } else {
      GrMockOptions mockOptions;
      gr_context_ = GrDirectContext::MakeMock(&mockOptions);
    }

    if (!raster_context_provider_) {
      // If there is no raster context provider, fall back to using a locally
      // created raster interface. Unit tests that want to use something other
      // than RasterImplementationGLES should pas a raster_context_provider.
      raster_interface_ =
          std::make_unique<gpu::raster::RasterImplementationGLES>(
              gl_, nullptr, capabilities_);
    }

    webgpu_interface_ = std::make_unique<gpu::webgpu::WebGPUInterfaceStub>();

    // enable all gpu features.
    for (unsigned feature = 0; feature < gpu::NUMBER_OF_GPU_FEATURE_TYPES;
         ++feature) {
      gpu_feature_info_.status_values[feature] = gpu::kGpuFeatureStatusEnabled;
    }
  }
  ~FakeWebGraphicsContext3DProvider() override = default;

  GrDirectContext* GetGrContext() override { return gr_context_.get(); }

  const gpu::Capabilities& GetCapabilities() const override {
    return capabilities_;
  }
  void SetCapabilities(const gpu::Capabilities& c) { capabilities_ = c; }

  const gpu::GpuFeatureInfo& GetGpuFeatureInfo() const override {
    return gpu_feature_info_;
  }

  const WebglPreferences& GetWebglPreferences() const override {
    return webgl_preferences_;
  }

  gpu::GLHelper* GetGLHelper() override { return nullptr; }

  gpu::InterfaceBase* InterfaceBase() override { return gl_; }
  gpu::gles2::GLES2Interface* ContextGL() override { return gl_; }
  gpu::raster::RasterInterface* RasterInterface() override {
    return raster_context_provider_
               ? raster_context_provider_->RasterInterface()
               : raster_interface_.get();
  }
  bool IsContextLost() override {
    return RasterInterface() &&
           RasterInterface()->GetGraphicsResetStatusKHR() != GL_NO_ERROR;
  }
  gpu::webgpu::WebGPUInterface* WebGPUInterface() override {
    return webgpu_interface_.get();
  }
  gpu::ContextSupport* ContextSupport() override { return nullptr; }

  bool BindToCurrentSequence() override { return false; }
  void SetLostContextCallback(base::RepeatingClosure) override {}
  void SetErrorMessageCallback(
      base::RepeatingCallback<void(const char*, int32_t id)>) override {}
  cc::ImageDecodeCache* ImageDecodeCache(SkColorType color_type) override {
    return image_decode_cache_;
  }
  viz::TestSharedImageInterface* SharedImageInterface() override {
    return &test_shared_image_interface_;
  }
  void CopyVideoFrame(media::PaintCanvasVideoRenderer* video_render,
                      media::VideoFrame* video_frame,
                      cc::PaintCanvas* canvas) override {}
  viz::RasterContextProvider* RasterContextProvider() const override {
    return raster_context_provider_;
  }

 private:
  cc::StubDecodeCache stub_image_decode_cache_;
  viz::TestSharedImageInterface test_shared_image_interface_;
  gpu::gles2::GLES2Interface* gl_;
  std::unique_ptr<gpu::raster::RasterInterface> raster_interface_;
  std::unique_ptr<gpu::webgpu::WebGPUInterfaceStub> webgpu_interface_;
  sk_sp<GrDirectContext> gr_context_;
  gpu::Capabilities capabilities_;
  gpu::GpuFeatureInfo gpu_feature_info_;
  WebglPreferences webgl_preferences_;
  cc::ImageDecodeCache* image_decode_cache_;
  viz::RasterContextProvider* raster_context_provider_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_TEST_FAKE_WEB_GRAPHICS_CONTEXT_3D_PROVIDER_H_
