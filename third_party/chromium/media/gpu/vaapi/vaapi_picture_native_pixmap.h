// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_GPU_VAAPI_VAAPI_PICTURE_NATIVE_PIXMAP_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_GPU_VAAPI_VAAPI_PICTURE_NATIVE_PIXMAP_H_

#include <stdint.h>

#include "base/memory/ref_counted.h"
#include "third_party/chromium/media/gpu/vaapi/va_surface.h"
#include "third_party/chromium/media/gpu/vaapi/vaapi_picture.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace gl {
class GLImage;
}

namespace media_m96 {

class VaapiWrapper;

// Implementation of VaapiPicture based on NativePixmaps.
class VaapiPictureNativePixmap : public VaapiPicture {
 public:
  VaapiPictureNativePixmap(
      scoped_refptr<VaapiWrapper> vaapi_wrapper,
      const MakeGLContextCurrentCallback& make_context_current_cb,
      const BindGLImageCallback& bind_image_cb_,
      int32_t picture_buffer_id,
      const gfx::Size& size,
      const gfx::Size& visible_size,
      uint32_t texture_id,
      uint32_t client_texture_id,
      uint32_t texture_target);

  VaapiPictureNativePixmap(const VaapiPictureNativePixmap&) = delete;
  VaapiPictureNativePixmap& operator=(const VaapiPictureNativePixmap&) = delete;

  ~VaapiPictureNativePixmap() override;

  // VaapiPicture implementation.
  bool DownloadFromSurface(scoped_refptr<VASurface> va_surface) override;
  bool AllowOverlay() const override;
  VASurfaceID va_surface_id() const override;

 protected:
  // GLImage bound to the GL textures used by the VDA client.
  scoped_refptr<gl::GLImage> gl_image_;

  // VASurface used to transfer from the decoder's pixel format.
  scoped_refptr<VASurface> va_surface_;
};

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_GPU_VAAPI_VAAPI_PICTURE_NATIVE_PIXMAP_H_
