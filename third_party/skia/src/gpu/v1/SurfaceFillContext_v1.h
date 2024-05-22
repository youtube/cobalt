/*
 * Copyright 2020 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SurfaceFillContext_v1_DEFINED
#define SurfaceFillContext_v1_DEFINED

#include "include/core/SkSize.h"
#include "include/private/GrTypesPriv.h"
#include "src/gpu/GrImageInfo.h"
#include "src/gpu/SurfaceFillContext.h"
#include "src/gpu/Swizzle.h"
#include "src/gpu/ops/OpsTask.h"

#include <array>
#include <tuple>

class GrFragmentProcessor;
class GrImageContext;
class GrOp;
class GrBackendFormat;
class GrRecordingContext;
class GrSurfaceProxyView;
class SkColorSpace;

namespace skgpu::v1 {

class SurfaceFillContext : public skgpu::SurfaceFillContext {
public:
    SurfaceFillContext(GrRecordingContext*,
                       GrSurfaceProxyView readView,
                       GrSurfaceProxyView writeView,
                       const GrColorInfo&,
                       bool flushTimeOpsTask = false);

    void discard() override;

    void resolveMSAA() override;

    void fillRectWithFP(const SkIRect& dstRect, std::unique_ptr<GrFragmentProcessor> fp) override;

    bool blitTexture(GrSurfaceProxyView view,
                     const SkIRect& srcRect,
                     const SkIPoint& dstPoint) override;

    OpsTask* getOpsTask();
    sk_sp<GrRenderTask> refRenderTask() override;

    int numSamples() const { return this->asRenderTargetProxy()->numSamples(); }
    bool wrapsVkSecondaryCB() const { return this->asRenderTargetProxy()->wrapsVkSecondaryCB(); }

    SkArenaAlloc* arenaAlloc() { return this->arenas()->arenaAlloc(); }
    GrSubRunAllocator* subRunAlloc() { return this->arenas()->subRunAlloc(); }

#if GR_TEST_UTILS
    OpsTask* testingOnly_PeekLastOpsTask() { return fOpsTask.get(); }
#endif

    const GrSurfaceProxyView& writeSurfaceView() const { return fWriteView; }

protected:
    /**
     * Creates a constant color paint for a clear, using src-over if possible to improve batching.
     */
    static void ClearToGrPaint(std::array<float, 4> color, GrPaint* paint);

    void addOp(GrOp::Owner);

    OpsTask* replaceOpsTask();

private:
    sk_sp<GrArenas> arenas() { return fWriteView.proxy()->asRenderTargetProxy()->arenas(); }

    /** Override to be notified in subclass before the current ops task is replaced. */
    virtual void willReplaceOpsTask(OpsTask* prevTask, OpsTask* nextTask) {}

    /**
     * Override to be called to participate in the decision to discard all previous ops if a
     * fullscreen clear occurs.
     */
    virtual OpsTask::CanDiscardPreviousOps canDiscardPreviousOpsOnFullClear() const {
        return OpsTask::CanDiscardPreviousOps::kYes;
    }

    void internalClear(const SkIRect* scissor,
                       std::array<float, 4> color,
                       bool upgradePartialToFull = false) override;

    void addDrawOp(GrOp::Owner);

    SkDEBUGCODE(void onValidate() const override;)

    // The OpsTask can be closed by some other surface context that has picked it up. For this
    // reason, the OpsTask should only ever be accessed via 'getOpsTask'.
    sk_sp<OpsTask> fOpsTask;

    bool fFlushTimeOpsTask;

    using INHERITED = skgpu::SurfaceFillContext;
};

} // namespace skgpu::v1

#endif // SurfaceFillContext_v1_DEFINED
