// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/test/gpu_test_utils.h"

#include "components/viz/test/test_context_provider.h"
#include "components/viz/test/test_gles2_interface.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/test/fake_web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

void InitializeSharedGpuContext(viz::TestContextProvider* test_context_provider,
                                cc::ImageDecodeCache* cache,
                                SetIsContextLost set_context_lost) {
  auto factory = [](viz::TestGLES2Interface* gl, GrDirectContext* context,
                    cc::ImageDecodeCache* cache,
                    viz::RasterContextProvider* raster_context_provider,
                    SetIsContextLost set_context_lost,
                    bool* gpu_compositing_disabled)
      -> std::unique_ptr<WebGraphicsContext3DProvider> {
    *gpu_compositing_disabled = false;

    if (set_context_lost == SetIsContextLost::kSetToFalse)
      gl->set_context_lost(false);
    else if (set_context_lost == SetIsContextLost::kSetToTrue)
      gl->set_context_lost(true);
    // else set_context_lost will not be modified

    auto context_provider = std::make_unique<FakeWebGraphicsContext3DProvider>(
        gl, cache, context, raster_context_provider);
    context_provider->SetCapabilities(gl->test_capabilities());
    return context_provider;
  };
  test_context_provider->BindToCurrentSequence();
  viz::TestGLES2Interface* gl = test_context_provider->TestContextGL();
  GrDirectContext* context = test_context_provider->GrContext();
  SharedGpuContext::SetContextProviderFactoryForTesting(WTF::BindRepeating(
      factory, WTF::Unretained(gl), WTF::Unretained(context),
      WTF::Unretained(cache), WTF::Unretained(test_context_provider),
      set_context_lost));
}

}  // namespace blink
