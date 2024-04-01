// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_picture_factory.h"

#include "base/containers/contains.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "media/video/picture.h"
#include "ui/base/ui_base_features.h"
#include "ui/gl/gl_bindings.h"

#if defined(USE_OZONE)
#include "media/gpu/vaapi/vaapi_picture_native_pixmap_ozone.h"
#endif  // defined(USE_OZONE)
#if BUILDFLAG(USE_VAAPI_X11)
#include "media/gpu/vaapi/vaapi_picture_native_pixmap_angle.h"
#include "media/gpu/vaapi/vaapi_picture_tfp.h"
#endif  // BUILDFLAG(USE_VAAPI_X11)
#if defined(USE_EGL)
#include "media/gpu/vaapi/vaapi_picture_native_pixmap_egl.h"
#endif

namespace media {

namespace {

template <typename PictureType>
std::unique_ptr<VaapiPicture> CreateVaapiPictureNativeImpl(
    scoped_refptr<VaapiWrapper> vaapi_wrapper,
    const MakeGLContextCurrentCallback& make_context_current_cb,
    const BindGLImageCallback& bind_image_cb,
    const PictureBuffer& picture_buffer,
    const gfx::Size& visible_size,
    uint32_t client_texture_id,
    uint32_t service_texture_id) {
  return std::make_unique<PictureType>(
      std::move(vaapi_wrapper), make_context_current_cb, bind_image_cb,
      picture_buffer.id(), picture_buffer.size(), visible_size,
      service_texture_id, client_texture_id, picture_buffer.texture_target());
}

}  // namespace

VaapiPictureFactory::VaapiPictureFactory() {
  vaapi_impl_pairs_.insert(
      std::make_pair(gl::kGLImplementationEGLGLES2,
                     VaapiPictureFactory::kVaapiImplementationDrm));
#if BUILDFLAG(USE_VAAPI_X11)
  vaapi_impl_pairs_.insert(
      std::make_pair(gl::kGLImplementationEGLANGLE,
                     VaapiPictureFactory::kVaapiImplementationAngle));
  vaapi_impl_pairs_.insert(
      std::make_pair(gl::kGLImplementationDesktopGL,
                     VaapiPictureFactory::kVaapiImplementationX11));
#endif

  DeterminePictureCreationAndDownloadingMechanism();
}

VaapiPictureFactory::~VaapiPictureFactory() = default;

std::unique_ptr<VaapiPicture> VaapiPictureFactory::Create(
    scoped_refptr<VaapiWrapper> vaapi_wrapper,
    const MakeGLContextCurrentCallback& make_context_current_cb,
    const BindGLImageCallback& bind_image_cb,
    const PictureBuffer& picture_buffer,
    const gfx::Size& visible_size) {
  // ARC++ sends |picture_buffer| with no texture_target().
  DCHECK(picture_buffer.texture_target() == GetGLTextureTarget() ||
         picture_buffer.texture_target() == 0u);

  // |client_texture_ids| and |service_texture_ids| are empty from ARC++.
  const uint32_t client_texture_id =
      !picture_buffer.client_texture_ids().empty()
          ? picture_buffer.client_texture_ids()[0]
          : 0;
  const uint32_t service_texture_id =
      !picture_buffer.service_texture_ids().empty()
          ? picture_buffer.service_texture_ids()[0]
          : 0;

  // Select DRM(egl) / TFP(glx) at runtime with --use-gl=egl / --use-gl=desktop
  return CreateVaapiPictureNative(vaapi_wrapper, make_context_current_cb,
                                  bind_image_cb, picture_buffer, visible_size,
                                  client_texture_id, service_texture_id);
}

VaapiPictureFactory::VaapiImplementation
VaapiPictureFactory::GetVaapiImplementation(gl::GLImplementation gl_impl) {
  if (base::Contains(vaapi_impl_pairs_, gl_impl))
    return vaapi_impl_pairs_[gl_impl];
  return kVaapiImplementationNone;
}

uint32_t VaapiPictureFactory::GetGLTextureTarget() {
#if BUILDFLAG(USE_VAAPI_X11)
  return GL_TEXTURE_2D;
#else
  return GL_TEXTURE_EXTERNAL_OES;
#endif
}

gfx::BufferFormat VaapiPictureFactory::GetBufferFormat() {
#if BUILDFLAG(USE_VAAPI_X11)
  return gfx::BufferFormat::RGBX_8888;
#else
  return gfx::BufferFormat::YUV_420_BIPLANAR;
#endif
}

void VaapiPictureFactory::DeterminePictureCreationAndDownloadingMechanism() {
  switch (GetVaapiImplementation(gl::GetGLImplementation())) {
#if defined(USE_OZONE)
    // We can be called without GL initialized, which is valid if we use Ozone.
    case kVaapiImplementationNone:
      create_picture_cb_ = base::BindRepeating(
          &CreateVaapiPictureNativeImpl<VaapiPictureNativePixmapOzone>);
      needs_vpp_for_downloading_ = true;
      break;
#endif  // defined(USE_OZONE)
#if BUILDFLAG(USE_VAAPI_X11)
    case kVaapiImplementationX11:
      create_picture_cb_ =
          base::BindRepeating(&CreateVaapiPictureNativeImpl<VaapiTFPPicture>);
      // Neither VaapiTFPPicture or VaapiPictureNativePixmapAngle needs the VPP.
      needs_vpp_for_downloading_ = false;
      break;
    case kVaapiImplementationAngle:
      create_picture_cb_ = base::BindRepeating(
          &CreateVaapiPictureNativeImpl<VaapiPictureNativePixmapAngle>);
      // Neither VaapiTFPPicture or VaapiPictureNativePixmapAngle needs the VPP.
      needs_vpp_for_downloading_ = false;
      break;
#endif  // BUILDFLAG(USE_VAAPI_X11)
    case kVaapiImplementationDrm:
#if defined(USE_OZONE)
      create_picture_cb_ = base::BindRepeating(
          &CreateVaapiPictureNativeImpl<VaapiPictureNativePixmapOzone>);
      needs_vpp_for_downloading_ = true;
      break;
#elif defined(USE_EGL)
      create_picture_cb_ = base::BindRepeating(
          &CreateVaapiPictureNativeImpl<VaapiPictureNativePixmapEgl>);
      needs_vpp_for_downloading_ = true;
      break;
#else
      // ozone or egl must be used to use the DRM implementation.
      FALLTHROUGH;
#endif
    default:
      NOTREACHED();
      break;
  }
}

bool VaapiPictureFactory::NeedsProcessingPipelineForDownloading() const {
  return needs_vpp_for_downloading_;
}

std::unique_ptr<VaapiPicture> VaapiPictureFactory::CreateVaapiPictureNative(
    scoped_refptr<VaapiWrapper> vaapi_wrapper,
    const MakeGLContextCurrentCallback& make_context_current_cb,
    const BindGLImageCallback& bind_image_cb,
    const PictureBuffer& picture_buffer,
    const gfx::Size& visible_size,
    uint32_t client_texture_id,
    uint32_t service_texture_id) {
  CHECK(create_picture_cb_);
  return create_picture_cb_.Run(
      std::move(vaapi_wrapper), make_context_current_cb, bind_image_cb,
      picture_buffer, visible_size, client_texture_id, service_texture_id);
}

}  // namespace media
