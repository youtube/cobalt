/*
 * Copyright 2018 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/core/SkCanvasPriv.h"

#include "src/core/SkAutoMalloc.h"
#include "src/core/SkDevice.h"
#include "src/core/SkReadBuffer.h"
#include "src/core/SkWriter32.h"

#include <locale>

SkAutoCanvasMatrixPaint::SkAutoCanvasMatrixPaint(SkCanvas* canvas, const SkMatrix* matrix,
                                                 const SkPaint* paint, const SkRect& bounds)
        : fCanvas(canvas)
        , fSaveCount(canvas->getSaveCount()) {
    if (paint) {
        SkRect newBounds = bounds;
        if (matrix) {
            matrix->mapRect(&newBounds);
        }
        canvas->saveLayer(&newBounds, paint);
    } else if (matrix) {
        canvas->save();
    }

    if (matrix) {
        canvas->concat(*matrix);
    }
}

SkAutoCanvasMatrixPaint::~SkAutoCanvasMatrixPaint() {
    fCanvas->restoreToCount(fSaveCount);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

bool SkCanvasPriv::ReadLattice(SkReadBuffer& buffer, SkCanvas::Lattice* lattice) {
    lattice->fXCount = buffer.readInt();
    lattice->fXDivs = buffer.skipT<int32_t>(lattice->fXCount);
    lattice->fYCount = buffer.readInt();
    lattice->fYDivs = buffer.skipT<int32_t>(lattice->fYCount);
    int flagCount = buffer.readInt();
    lattice->fRectTypes = nullptr;
    lattice->fColors = nullptr;
    if (flagCount) {
        lattice->fRectTypes = buffer.skipT<SkCanvas::Lattice::RectType>(flagCount);
        lattice->fColors = buffer.skipT<SkColor>(flagCount);
    }
    lattice->fBounds = buffer.skipT<SkIRect>();
    return buffer.isValid();
}

size_t SkCanvasPriv::WriteLattice(void* buffer, const SkCanvas::Lattice& lattice) {
    int flagCount = lattice.fRectTypes ? (lattice.fXCount + 1) * (lattice.fYCount + 1) : 0;

    const size_t size = (1 + lattice.fXCount + 1 + lattice.fYCount + 1) * sizeof(int32_t) +
                        SkAlign4(flagCount * sizeof(SkCanvas::Lattice::RectType)) +
                        SkAlign4(flagCount * sizeof(SkColor)) +
                        sizeof(SkIRect);

    if (buffer) {
        SkWriter32 writer(buffer, size);
        writer.write32(lattice.fXCount);
        writer.write(lattice.fXDivs, lattice.fXCount * sizeof(uint32_t));
        writer.write32(lattice.fYCount);
        writer.write(lattice.fYDivs, lattice.fYCount * sizeof(uint32_t));
        writer.write32(flagCount);
        writer.writePad(lattice.fRectTypes, flagCount * sizeof(uint8_t));
        writer.write(lattice.fColors, flagCount * sizeof(SkColor));
        SkASSERT(lattice.fBounds);
        writer.write(lattice.fBounds, sizeof(SkIRect));
        SkASSERT(writer.bytesWritten() == size);
    }
    return size;
};

void SkCanvasPriv::WriteLattice(SkWriteBuffer& buffer, const SkCanvas::Lattice& lattice) {
    const size_t size = WriteLattice(nullptr, lattice);
    SkAutoSMalloc<1024> storage(size);
    WriteLattice(storage.get(), lattice);
    buffer.writePad32(storage.get(), size);
}

void SkCanvasPriv::GetDstClipAndMatrixCounts(const SkCanvas::ImageSetEntry set[], int count,
                                             int* totalDstClipCount, int* totalMatrixCount) {
    int dstClipCount = 0;
    int maxMatrixIndex = -1;
    for (int i = 0; i < count; ++i) {
        dstClipCount += 4 * set[i].fHasClip;
        if (set[i].fMatrixIndex > maxMatrixIndex) {
            maxMatrixIndex = set[i].fMatrixIndex;
        }
    }

    *totalDstClipCount = dstClipCount;
    *totalMatrixCount = maxMatrixIndex + 1;
}

#ifdef SK_ENABLE_SKSL
void SkCanvasPriv::DrawCustomMesh(SkCanvas* canvas,
                                  SkCustomMesh cm,
                                  sk_sp<SkBlender> blender,
                                  const SkPaint& paint) {
    canvas->drawCustomMesh(cm, std::move(blender), paint);
}
#endif

#if GR_TEST_UTILS

#if SK_SUPPORT_GPU
#include "src/gpu/BaseDevice.h"

#if SK_GPU_V1
skgpu::v1::SurfaceDrawContext* SkCanvasPriv::TopDeviceSurfaceDrawContext(SkCanvas* canvas) {
    if (auto gpuDevice = canvas->topDevice()->asGpuDevice()) {
        return gpuDevice->surfaceDrawContext();
    }

    return nullptr;
}
#endif // SK_GPU_V1

skgpu::SurfaceFillContext* SkCanvasPriv::TopDeviceSurfaceFillContext(SkCanvas* canvas) {
    if (auto gpuDevice = canvas->topDevice()->asGpuDevice()) {
        return gpuDevice->surfaceFillContext();
    }

    return nullptr;
}

#else // SK_SUPPORT_GPU

#if SK_GPU_V1
skgpu::v1::SurfaceDrawContext* SkCanvasPriv::TopDeviceSurfaceDrawContext(SkCanvas* canvas) {
    return nullptr;
}
#endif // SK_GPU_V1

skgpu::SurfaceFillContext* SkCanvasPriv::TopDeviceSurfaceFillContext(SkCanvas* canvas) {
    return nullptr;
}

#endif // SK_SUPPORT_GPU

#endif // GR_TEST_UTILS

#if SK_SUPPORT_GPU
#include "src/gpu/BaseDevice.h"

GrRenderTargetProxy* SkCanvasPriv::TopDeviceTargetProxy(SkCanvas* canvas) {
    if (auto gpuDevice = canvas->topDevice()->asGpuDevice()) {
        return gpuDevice->targetProxy();
    }

    return nullptr;
}

#else // SK_SUPPORT_GPU

GrRenderTargetProxy* SkCanvasPriv::TopDeviceTargetProxy(SkCanvas* canvas) {
    return nullptr;
}

#endif // SK_SUPPORT_GPU
