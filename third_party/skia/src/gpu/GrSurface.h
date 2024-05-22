/*
 * Copyright 2012 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrSurface_DEFINED
#define GrSurface_DEFINED

#include "include/core/SkImageInfo.h"
#include "include/core/SkRect.h"
#include "include/gpu/GrBackendSurface.h"
#include "include/gpu/GrTypes.h"
#include "src/gpu/GrGpuResource.h"

class GrRenderTarget;
class GrTexture;

class GrSurface : public GrGpuResource {
public:
    /**
     * Retrieves the dimensions of the surface.
     */
    SkISize dimensions() const { return fDimensions; }

    /**
     * Retrieves the width of the surface.
     */
    int width() const { return fDimensions.width(); }

    /**
     * Retrieves the height of the surface.
     */
    int height() const { return fDimensions.height(); }

    /**
     * Helper that gets the width and height of the surface as a bounding rectangle.
     */
    SkRect getBoundsRect() const { return SkRect::Make(this->dimensions()); }

    virtual GrBackendFormat backendFormat() const = 0;

    void setRelease(sk_sp<GrRefCntedCallback> releaseHelper) {
        this->onSetRelease(releaseHelper);
        fReleaseHelper = std::move(releaseHelper);
    }

    // These match the definitions in SkImage, from whence they came.
    // TODO: Remove Chrome's need to call this on a GrTexture
    typedef void* ReleaseCtx;
    typedef void (*ReleaseProc)(ReleaseCtx);
    void setRelease(ReleaseProc proc, ReleaseCtx ctx) {
        this->setRelease(GrRefCntedCallback::Make(proc, ctx));
    }

    /**
     * @return the texture associated with the surface, may be null.
     */
    virtual GrTexture* asTexture() { return nullptr; }
    virtual const GrTexture* asTexture() const { return nullptr; }

    /**
     * @return the render target underlying this surface, may be null.
     */
    virtual GrRenderTarget* asRenderTarget() { return nullptr; }
    virtual const GrRenderTarget* asRenderTarget() const { return nullptr; }

    GrInternalSurfaceFlags flags() const { return fSurfaceFlags; }

    static size_t ComputeSize(const GrBackendFormat&, SkISize dimensions, int colorSamplesPerPixel,
                              GrMipmapped, bool binSize = false);

    /**
     * The pixel values of this surface cannot be modified (e.g. doesn't support write pixels or
     * MIP map level regen).
     */
    bool readOnly() const { return fSurfaceFlags & GrInternalSurfaceFlags::kReadOnly; }

    bool framebufferOnly() const {
        return fSurfaceFlags & GrInternalSurfaceFlags::kFramebufferOnly;
    }

    // Returns true if we are working with protected content.
    bool isProtected() const { return fIsProtected == GrProtected::kYes; }

    void setFramebufferOnly() {
        SkASSERT(this->asRenderTarget());
        fSurfaceFlags |= GrInternalSurfaceFlags::kFramebufferOnly;
    }

protected:
    void setGLRTFBOIDIs0() {
        SkASSERT(!this->requiresManualMSAAResolve());
        SkASSERT(!this->asTexture());
        SkASSERT(this->asRenderTarget());
        fSurfaceFlags |= GrInternalSurfaceFlags::kGLRTFBOIDIs0;
    }
    bool glRTFBOIDis0() const {
        return fSurfaceFlags & GrInternalSurfaceFlags::kGLRTFBOIDIs0;
    }

    void setRequiresManualMSAAResolve() {
        SkASSERT(!this->glRTFBOIDis0());
        SkASSERT(this->asRenderTarget());
        fSurfaceFlags |= GrInternalSurfaceFlags::kRequiresManualMSAAResolve;
    }
    bool requiresManualMSAAResolve() const {
        return fSurfaceFlags & GrInternalSurfaceFlags::kRequiresManualMSAAResolve;
    }

    void setReadOnly() {
        SkASSERT(!this->asRenderTarget());
        fSurfaceFlags |= GrInternalSurfaceFlags::kReadOnly;
    }

    void setVkRTSupportsInputAttachment() {
        SkASSERT(this->asRenderTarget());
        fSurfaceFlags |= GrInternalSurfaceFlags::kVkRTSupportsInputAttachment;
    }

    GrSurface(GrGpu* gpu, const SkISize& dimensions, GrProtected isProtected)
            : INHERITED(gpu)
            , fDimensions(dimensions)
            , fSurfaceFlags(GrInternalSurfaceFlags::kNone)
            , fIsProtected(isProtected) {}

    ~GrSurface() override {
        // check that invokeReleaseProc has been called (if needed)
        SkASSERT(!fReleaseHelper);
    }

    void onRelease() override;
    void onAbandon() override;

private:
    const char* getResourceType() const override { return "Surface"; }

    // Unmanaged backends (e.g. Vulkan) may want to specially handle the release proc in order to
    // ensure it isn't called until GPU work related to the resource is completed.
    virtual void onSetRelease(sk_sp<GrRefCntedCallback>) {}

    void invokeReleaseProc() {
        // Depending on the ref count of fReleaseHelper this may or may not actually trigger the
        // ReleaseProc to be called.
        fReleaseHelper.reset();
    }

    SkISize                    fDimensions;
    GrInternalSurfaceFlags     fSurfaceFlags;
    GrProtected                fIsProtected;
    sk_sp<GrRefCntedCallback>  fReleaseHelper;

    using INHERITED = GrGpuResource;
};

#endif
