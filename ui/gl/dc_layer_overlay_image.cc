// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/dc_layer_overlay_image.h"

#include <unknwn.h>

#include <d3d11.h>
#include <dcomp.h>

namespace gl {

DCLayerOverlayImage::DCLayerOverlayImage(
    const gfx::Size& size,
    Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_video_texture,
    size_t array_slice)
    : type_(DCLayerOverlayType::kD3D11Texture),
      size_(size),
      d3d11_video_texture_(std::move(d3d11_video_texture)),
      texture_array_slice_(array_slice) {}

DCLayerOverlayImage::DCLayerOverlayImage(const gfx::Size& size,
                                         const uint8_t* shm_video_pixmap,
                                         size_t pixmap_stride)
    : type_(DCLayerOverlayType::kShMemPixmap),
      size_(size),
      shm_video_pixmap_(shm_video_pixmap),
      pixmap_stride_(pixmap_stride) {}

DCLayerOverlayImage::DCLayerOverlayImage(
    const gfx::Size& size,
    Microsoft::WRL::ComPtr<IUnknown> dcomp_visual_content,
    uint64_t dcomp_surface_serial)
    : type_(DCLayerOverlayType::kDCompVisualContent),
      size_(size),
      dcomp_visual_content_(std::move(dcomp_visual_content)),
      dcomp_surface_serial_(dcomp_surface_serial) {}

DCLayerOverlayImage::DCLayerOverlayImage(
    const gfx::Size& size,
    scoped_refptr<gl::DCOMPSurfaceProxy> dcomp_surface_proxy)
    : type_(DCLayerOverlayType::kDCompSurfaceProxy),
      size_(size),
      dcomp_surface_proxy_(std::move(dcomp_surface_proxy)) {}

DCLayerOverlayImage::DCLayerOverlayImage(DCLayerOverlayImage&&) = default;

DCLayerOverlayImage& DCLayerOverlayImage::operator=(DCLayerOverlayImage&&) =
    default;

DCLayerOverlayImage::~DCLayerOverlayImage() = default;

}  // namespace gl
