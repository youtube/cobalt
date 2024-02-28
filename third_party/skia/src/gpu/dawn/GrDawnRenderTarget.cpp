/*
 * Copyright 2019 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/dawn/GrDawnRenderTarget.h"

#include "include/gpu/GrBackendSurface.h"
#include "src/gpu/dawn/GrDawnGpu.h"
#include "src/gpu/dawn/GrDawnUtil.h"

GrDawnRenderTarget::GrDawnRenderTarget(GrDawnGpu* gpu,
                                       SkISize dimensions,
                                       int sampleCnt,
                                       const GrDawnRenderTargetInfo& info)
        : GrSurface(gpu, dimensions, GrProtected::kNo)
        , GrRenderTarget(gpu, dimensions, sampleCnt, GrProtected::kNo)
        , fInfo(info) {}

sk_sp<GrDawnRenderTarget> GrDawnRenderTarget::MakeWrapped(GrDawnGpu* gpu,
                                                          SkISize dimensions,
                                                          int sampleCnt,
                                                          const GrDawnRenderTargetInfo& info) {
    sk_sp<GrDawnRenderTarget> rt(new GrDawnRenderTarget(gpu, dimensions, sampleCnt, info));
    rt->registerWithCacheWrapped(GrWrapCacheable::kNo);
    return rt;
}

size_t GrDawnRenderTarget::onGpuMemorySize() const {
    // The plus 1 is to account for the resolve texture or if not using msaa the RT itself
    int numSamples = this->numSamples() + 1;
    return GrSurface::ComputeSize(this->backendFormat(), this->dimensions(), numSamples,
                                  GrMipmapped::kNo);
}

bool GrDawnRenderTarget::completeStencilAttachment(GrAttachment* stencil, bool useMSAASurface) {
    SkASSERT(useMSAASurface == (this->numSamples() > 1));
    return true;
}

GrDawnRenderTarget::~GrDawnRenderTarget() {
}

void GrDawnRenderTarget::onRelease() {
    INHERITED::onRelease();
}

void GrDawnRenderTarget::onAbandon() {
    INHERITED::onAbandon();
}

GrBackendRenderTarget GrDawnRenderTarget::getBackendRenderTarget() const {
    return GrBackendRenderTarget(this->width(), this->height(), this->numSamples(),
                                 this->numSamples(), fInfo);
}

GrBackendFormat GrDawnRenderTarget::backendFormat() const {
    return GrBackendFormat::MakeDawn(fInfo.fFormat);
}
