/*
 * Copyright 2021 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "experimental/graphite/src/DrawContext.h"

#include "include/core/SkPixmap.h"
#include "include/private/SkColorData.h"

#include "experimental/graphite/include/Context.h"
#include "experimental/graphite/include/Recorder.h"
#include "experimental/graphite/src/Buffer.h"
#include "experimental/graphite/src/Caps.h"
#include "experimental/graphite/src/CommandBuffer.h"
#include "experimental/graphite/src/ContextPriv.h"
#include "experimental/graphite/src/DrawList.h"
#include "experimental/graphite/src/DrawPass.h"
#include "experimental/graphite/src/Gpu.h"
#include "experimental/graphite/src/RecorderPriv.h"
#include "experimental/graphite/src/RenderPassTask.h"
#include "experimental/graphite/src/ResourceTypes.h"
#include "experimental/graphite/src/TextureProxy.h"
#include "experimental/graphite/src/UploadTask.h"
#include "experimental/graphite/src/geom/BoundsManager.h"
#include "experimental/graphite/src/geom/Shape.h"

namespace skgpu {

sk_sp<DrawContext> DrawContext::Make(sk_sp<TextureProxy> target,
                                     sk_sp<SkColorSpace> colorSpace,
                                     SkColorType colorType,
                                     SkAlphaType alphaType) {
    if (!target) {
        return nullptr;
    }

    // TODO: validate that the color type and alpha type are compatible with the target's info
    SkImageInfo imageInfo = SkImageInfo::Make(target->dimensions(),
                                              colorType,
                                              alphaType,
                                              std::move(colorSpace));
    return sk_sp<DrawContext>(new DrawContext(std::move(target), imageInfo));
}

DrawContext::DrawContext(sk_sp<TextureProxy> target, const SkImageInfo& ii)
        : fTarget(std::move(target))
        , fImageInfo(ii)
        , fPendingDraws(std::make_unique<DrawList>())
        , fPendingUploads(std::make_unique<UploadList>()) {
    // TBD - Will probably want DrawLists (and its internal commands) to come from an arena
    // that the DC manages.
}

DrawContext::~DrawContext() {
    // If the DC is destroyed and there are pending commands, they won't be drawn.
    fPendingDraws.reset();
    fDrawPasses.clear();
}

void DrawContext::stencilAndFillPath(const Transform& localToDevice,
                                     const Shape& shape,
                                     const Clip& clip,
                                     DrawOrder order,
                                     const PaintParams* paint)  {
    SkASSERT(SkIRect::MakeSize(fTarget->dimensions()).contains(clip.scissor()));
    fPendingDraws->stencilAndFillPath(localToDevice, shape, clip, order,paint);
}

void DrawContext::fillConvexPath(const Transform& localToDevice,
                                 const Shape& shape,
                                 const Clip& clip,
                                 DrawOrder order,
                                 const PaintParams* paint) {
    SkASSERT(SkIRect::MakeSize(fTarget->dimensions()).contains(clip.scissor()));
    fPendingDraws->fillConvexPath(localToDevice, shape, clip, order, paint);
}

void DrawContext::strokePath(const Transform& localToDevice,
                             const Shape& shape,
                             const StrokeParams& stroke,
                             const Clip& clip,
                             DrawOrder order,
                             const PaintParams* paint) {
    SkASSERT(SkIRect::MakeSize(fTarget->dimensions()).contains(clip.scissor()));
    fPendingDraws->strokePath(localToDevice, shape, stroke, clip, order, paint);
}

void DrawContext::clear(const SkColor4f& clearColor) {
    fPendingLoadOp = LoadOp::kClear;
    SkPMColor4f pmColor = clearColor.premul();
    fPendingClearColor = pmColor.array();

    // a fullscreen clear will overwrite anything that came before, so start a new DrawList
    // and clear any drawpasses that haven't been snapped yet
    fPendingDraws = std::make_unique<DrawList>();
    fDrawPasses.clear();
}

//
// TODO: The other draw-recording APIs in DrawContext are relatively simple, just storing state
// from the caller's decision making. If possible we should consider moving the more complex logic
// somewhere above DrawContext and have this be much simpler.
bool DrawContext::writePixels(Recorder* recorder, const SkPixmap& src, SkIPoint dstPoint) {
    // TODO: add mipmap support for createBackendTexture

    // Our caller should have clipped to the bounds of the surface already.
    SkASSERT(SkIRect::MakeSize(fTarget->dimensions()).contains(
            SkIRect::MakePtSize(dstPoint, src.dimensions())));

    if (!recorder) {
        return false;
    }

    if (src.colorType() == kUnknown_SkColorType) {
        return false;
    }

    // TODO: check for readOnly or framebufferOnly target and return false if so

    const Caps* caps = recorder->priv().caps();

    // TODO: canvas2DFastPath?
    // TODO: check that surface supports writePixels
    // TODO: handle writePixels as draw if needed (e.g., canvas2DFastPath || !supportsWritePixels)

    // TODO: check for flips and conversions and either handle here or pass info to appendUpload

    // for now, until conversions are supported
    if (!caps->areColorTypeAndTextureInfoCompatible(src.colorType(),
                                                    fTarget->textureInfo())) {
        return false;
    }

    std::vector<MipLevel> levels;
    levels.push_back({src.addr(), src.rowBytes()});

    SkIRect dstRect = SkIRect::MakePtSize(dstPoint, src.dimensions());
    return fPendingUploads->appendUpload(recorder,
                                         fTarget,
                                         src.colorType(),
                                         levels,
                                         dstRect);
}

void DrawContext::snapDrawPass(Recorder* recorder, const BoundsManager* occlusionCuller) {
    if (fPendingDraws->drawCount() == 0) {
        return;
    }

    auto pass = DrawPass::Make(recorder, std::move(fPendingDraws), fTarget,
                               std::make_pair(fPendingLoadOp, fPendingStoreOp), fPendingClearColor,
                               occlusionCuller);
    fDrawPasses.push_back(std::move(pass));
    fPendingDraws = std::make_unique<DrawList>();
    fPendingLoadOp = LoadOp::kLoad;
    fPendingStoreOp = StoreOp::kStore;
}

sk_sp<Task> DrawContext::snapRenderPassTask(Recorder* recorder,
                                            const BoundsManager* occlusionCuller) {
    this->snapDrawPass(recorder, occlusionCuller);
    if (fDrawPasses.empty()) {
        return nullptr;
    }

    // TODO: At this point we would determine all the targets used by the drawPasses,
    // build up the union of them and store them in the RenderPassDesc. However, for
    // the moment we should have only one drawPass.
    SkASSERT(fDrawPasses.size() == 1);
    RenderPassDesc desc;
    auto& drawPass = fDrawPasses[0];
    desc.fColorAttachment.fTextureInfo = drawPass->target()->textureInfo();
    std::tie(desc.fColorAttachment.fLoadOp, desc.fColorAttachment.fStoreOp) = drawPass->ops();
    desc.fClearColor = drawPass->clearColor();

    if (drawPass->depthStencilFlags() != DepthStencilFlags::kNone) {
        const Caps* caps = recorder->priv().caps();
        desc.fDepthStencilAttachment.fTextureInfo =
                caps->getDefaultDepthStencilTextureInfo(drawPass->depthStencilFlags(),
                                                        1 /*sampleCount*/, // TODO: MSAA
                                                        Protected::kNo);
        // Always clear the depth and stencil to 0 at the start of a DrawPass, but discard at the
        // end since their contents do not affect the next frame.
        desc.fDepthStencilAttachment.fLoadOp = LoadOp::kClear;
        desc.fClearDepth = 0.f;
        desc.fClearStencil = 0;
        desc.fDepthStencilAttachment.fStoreOp = StoreOp::kDiscard;
    }

    sk_sp<TextureProxy> targetProxy = sk_ref_sp(fDrawPasses[0]->target());
    return RenderPassTask::Make(std::move(fDrawPasses), desc, std::move(targetProxy));
}

sk_sp<Task> DrawContext::snapUploadTask(Recorder* recorder) {
    if (!fPendingUploads) {
        return nullptr;
    }

    sk_sp<Task> uploadTask = UploadTask::Make(fPendingUploads.get());

    fPendingUploads = std::make_unique<UploadList>();

    return uploadTask;
}

} // namespace skgpu
