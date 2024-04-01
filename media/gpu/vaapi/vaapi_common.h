// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef MEDIA_GPU_VAAPI_VAAPI_COMMON_H_
#define MEDIA_GPU_VAAPI_VAAPI_COMMON_H_

#include "build/chromeos_buildflags.h"
#include "media/gpu/av1_picture.h"
#include "media/gpu/h264_dpb.h"
#include "media/gpu/vaapi/va_surface.h"
#include "media/gpu/vp8_picture.h"
#include "media/gpu/vp9_picture.h"
#include "media/media_buildflags.h"

#if BUILDFLAG(ENABLE_PLATFORM_HEVC_DECODING)
#include "media/gpu/h265_dpb.h"
#endif

namespace media {

// These picture classes derive from platform-independent, codec-specific
// classes to allow augmenting them with VA-API-specific traits; used when
// providing associated hardware resources and parameters to VA-API drivers.

class VaapiH264Picture : public H264Picture {
 public:
  explicit VaapiH264Picture(scoped_refptr<VASurface> va_surface);

  VaapiH264Picture* AsVaapiH264Picture() override;

  scoped_refptr<VASurface> va_surface() const { return va_surface_; }
  VASurfaceID GetVASurfaceID() const { return va_surface_->id(); }

 protected:
  ~VaapiH264Picture() override;

 private:
  scoped_refptr<VASurface> va_surface_;

  DISALLOW_COPY_AND_ASSIGN(VaapiH264Picture);
};

#if BUILDFLAG(ENABLE_PLATFORM_HEVC_DECODING)
class VaapiH265Picture : public H265Picture {
 public:
  explicit VaapiH265Picture(scoped_refptr<VASurface> va_surface);

  VaapiH265Picture(const VaapiH265Picture&) = delete;
  VaapiH265Picture& operator=(const VaapiH265Picture&) = delete;

  VaapiH265Picture* AsVaapiH265Picture() override;

  scoped_refptr<VASurface> va_surface() const { return va_surface_; }
  VASurfaceID GetVASurfaceID() const { return va_surface_->id(); }

 protected:
  ~VaapiH265Picture() override;

 private:
  scoped_refptr<VASurface> va_surface_;
};
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC_DECODING)

class VaapiVP8Picture : public VP8Picture {
 public:
  explicit VaapiVP8Picture(scoped_refptr<VASurface> va_surface);

  VaapiVP8Picture* AsVaapiVP8Picture() override;

  scoped_refptr<VASurface> va_surface() const { return va_surface_; }
  VASurfaceID GetVASurfaceID() const { return va_surface_->id(); }

 protected:
  ~VaapiVP8Picture() override;

 private:
  scoped_refptr<VASurface> va_surface_;

  DISALLOW_COPY_AND_ASSIGN(VaapiVP8Picture);
};

class VaapiVP9Picture : public VP9Picture {
 public:
  explicit VaapiVP9Picture(scoped_refptr<VASurface> va_surface);

  VaapiVP9Picture* AsVaapiVP9Picture() override;

  scoped_refptr<VASurface> va_surface() const { return va_surface_; }
  VASurfaceID GetVASurfaceID() const { return va_surface_->id(); }

 protected:
  ~VaapiVP9Picture() override;

 private:
  scoped_refptr<VP9Picture> CreateDuplicate() override;

  scoped_refptr<VASurface> va_surface_;

  DISALLOW_COPY_AND_ASSIGN(VaapiVP9Picture);
};

class VaapiAV1Picture : public AV1Picture {
 public:
  VaapiAV1Picture(scoped_refptr<VASurface> display_va_surface,
                  scoped_refptr<VASurface> reconstruct_va_surface);
  VaapiAV1Picture(const VaapiAV1Picture&) = delete;
  VaapiAV1Picture& operator=(const VaapiAV1Picture&) = delete;

  const scoped_refptr<VASurface>& display_va_surface() const {
    return display_va_surface_;
  }
  const scoped_refptr<VASurface>& reconstruct_va_surface() const {
    return reconstruct_va_surface_;
  }

 protected:
  ~VaapiAV1Picture() override;

 private:
  scoped_refptr<AV1Picture> CreateDuplicate() override;

  // |display_va_surface_| refers to the final decoded frame, both when using
  // film grain synthesis and when not using film grain.
  // |reconstruct_va_surface_| is only useful when using film grain synthesis:
  // it's the decoded frame prior to applying the film grain.
  // When not using film grain synthesis, |reconstruct_va_surface_| is equal to
  // |display_va_surface_|. This is necessary to simplify the reference frame
  // code when filling the VA-API structures and to be able to always use
  // reconstruct_va_surface() when calling ExecuteAndDestroyPendingBuffers()
  // (the driver expects the reconstructed surface as the target in the case
  // of film grain synthesis).
  scoped_refptr<VASurface> display_va_surface_;
  scoped_refptr<VASurface> reconstruct_va_surface_;
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VAAPI_COMMON_H_
